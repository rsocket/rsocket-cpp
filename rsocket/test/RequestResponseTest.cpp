// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/synchronization/Baton.h>
#include <gtest/gtest.h>
#include <thread>

#include "RSocketTests.h"
#include "rsocket/test/test_utils/GenericRequestResponseHandler.h"
#include "yarpl/Single.h"
#include "yarpl/single/SingleTestObserver.h"

using namespace yarpl::single;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

namespace {
class TestHandlerCancel : public rsocket::RSocketResponder {
 public:
  TestHandlerCancel(
      std::shared_ptr<folly::Baton<>> onCancel,
      std::shared_ptr<folly::Baton<>> onSubscribe)
      : onCancel_(std::move(onCancel)), onSubscribe_(std::move(onSubscribe)) {}
  std::shared_ptr<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId) override {
    // used to signal to the client when the subscribe is received
    onSubscribe_->post();
    // used to block this responder thread until a cancel is sent from client
    // over network
    auto cancelFromClient = std::make_shared<folly::Baton<>>();
    // used to signal to the client once we receive a cancel
    auto onCancel = onCancel_;
    auto requestString = request.moveDataToString();
    return Single<Payload>::create([name = std::move(requestString),
                                    cancelFromClient,
                                    onCancel](auto subscriber) mutable {
      std::thread([subscriber = std::move(subscriber),
                   name = std::move(name),
                   cancelFromClient,
                   onCancel]() {
        auto subscription = SingleSubscriptions::create(
            [cancelFromClient] { cancelFromClient->post(); });
        subscriber->onSubscribe(subscription);
        // simulate slow processing or IO being done
        // and block this current background thread
        // until we are cancelled
        cancelFromClient->timed_wait(std::chrono::seconds(1));
        if (subscription->isCancelled()) {
          //  this is used by the unit test to assert the cancel was
          //  received
          onCancel->post();
        } else {
          // if not cancelled would do work and emit here
        }
      }).detach();
    });
  }

 private:
  std::shared_ptr<folly::Baton<>> onCancel_;
  std::shared_ptr<folly::Baton<>> onSubscribe_;
};
} // namespace

TEST(RequestResponseTest, Cancel) {
  folly::ScopedEventBaseThread worker;
  auto onCancel = std::make_shared<folly::Baton<>>();
  auto onSubscribe = std::make_shared<folly::Baton<>>();
  auto server =
      makeServer(std::make_shared<TestHandlerCancel>(onCancel, onSubscribe));
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto to = SingleTestObserver<std::string>::create();
  requester->requestResponse(Payload("Jane"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(to);
  // NOTE: wait for server to receive request/subscribe
  // otherwise the cancellation will all happen locally
  onSubscribe->wait();
  // now cancel the local subscription
  to->cancel();
  // wait for cancel to propagate to server
  onCancel->wait();
  // assert no signals received on client
  to->assertNoTerminalEvent();
}

// response creation usage
TEST(RequestResponseTest, CanCtorTypes) {
  Response r1 = payload_response("foo", "bar");
  Response r2 = error_response(std::runtime_error("whew!"));
}

TEST(RequestResponseTest, Hello) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<GenericRequestResponseHandler>(
      [](StringPair const& request) {
        return payload_response(
            "Hello, " + request.first + " " + request.second + "!", ":)");
      }));

  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto to = SingleTestObserver<StringPair>::create();
  requester->requestResponse(Payload("Jane", "Doe"))
      ->map(payload_to_stringpair)
      ->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue({"Hello, Jane Doe!", ":)"});
}

TEST(RequestResponseTest, FailureInResponse) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<GenericRequestResponseHandler>(
      [](StringPair const& request) {
        EXPECT_EQ(request.first, "foo");
        EXPECT_EQ(request.second, "bar");
        return error_response(std::runtime_error("whew!"));
      }));

  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto to = SingleTestObserver<StringPair>::create();
  requester->requestResponse(Payload("foo", "bar"))
      ->map(payload_to_stringpair)
      ->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnErrorMessage("ErrorWithPayload");
  EXPECT_TRUE(to->getException().with_exception([](ErrorWithPayload& err) {
    EXPECT_STREQ("whew!", err.payload.moveDataToString().c_str());
  }));
}

TEST(RequestResponseTest, RequestOnDisconnectedClient) {
  folly::ScopedEventBaseThread worker;
  auto client = makeDisconnectedClient(worker.getEventBase());

  auto requester = client->getRequester();
  bool did_call_on_error = false;
  folly::Baton<> wait_for_on_error;
  requester->requestResponse(Payload("foo", "bar"))
      ->subscribe(
          [](auto) {
            // should not call onSuccess
            FAIL();
          },
          [&](folly::exception_wrapper) {
            did_call_on_error = true;
            wait_for_on_error.post();
          });

  wait_for_on_error.timed_wait(std::chrono::milliseconds(100));
  ASSERT_TRUE(did_call_on_error);
}

// TODO: test that multiple requests on a requestResponse
// fail in a well-defined way (right now it'd nullptr deref)
TEST(RequestResponseTest, MultipleRequestsError) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<GenericRequestResponseHandler>(
      [](StringPair const& request) {
        EXPECT_EQ(request.first, "foo");
        EXPECT_EQ(request.second, "bar");
        return payload_response("baz", "quix");
      }));

  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto flowable = requester->requestResponse(Payload("foo", "bar"));
}

TEST(RequestResponseTest, FailureOnRequest) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(
      std::make_shared<GenericRequestResponseHandler>([](auto const&) {
        ADD_FAILURE();
        return payload_response("", "");
      }));

  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  VLOG(0) << "Shutting down server so client request fails";
  server->shutdownAndWait();
  server.reset();
  VLOG(0) << "Done";

  auto to = SingleTestObserver<StringPair>::create();
  requester->requestResponse(Payload("foo", "bar"))
      ->map(payload_to_stringpair)
      ->subscribe(to);
  to->awaitTerminalEvent();
  EXPECT_TRUE(to->getError());
}

struct LargePayloadReqRespHandler : public rsocket::RSocketResponder {
  LargePayloadReqRespHandler(std::string const& data, std::string const& meta)
      : data(data), meta(meta) {}

  std::shared_ptr<yarpl::single::Single<Payload>> handleRequestResponse(
      Payload payload,
      StreamId) override {
    RSocketPayloadUtils::checkSameStrings(
        payload.data, data, "data received in payload");
    RSocketPayloadUtils::checkSameStrings(
        payload.metadata, meta, "metadata received in payload");

    return yarpl::single::Single<Payload>::create(
        [p = std::move(payload)](auto sub) mutable {
          sub->onSubscribe(yarpl::single::SingleSubscriptions::empty());
          sub->onSuccess(std::move(p));
        });
  }

  std::string const& data;
  std::string const& meta;
};

TEST(RequestResponseTest, TestLargePayload) {
  VLOG(1) << "Building up large data/metadata, this may take a moment...";
  std::string niceLongData = RSocketPayloadUtils::makeLongString(
      RSocketPayloadUtils::LargeRequestSize, "ABCDEFGH");
  std::string niceLongMeta = RSocketPayloadUtils::makeLongString(
      RSocketPayloadUtils::LargeRequestSize, "12345678");
  VLOG(1) << "Built meta size: " << niceLongMeta.size()
          << " data size: " << niceLongData.size();

  auto checkForSizePattern = [&](std::vector<size_t> const& meta_sizes,
                                 std::vector<size_t> const& data_sizes) {
    folly::ScopedEventBaseThread worker;
    auto server = makeServer(std::make_shared<LargePayloadReqRespHandler>(
        niceLongData, niceLongMeta));
    auto client = makeClient(worker.getEventBase(), *server->listeningPort());
    auto requester = client->getRequester();
    auto to = SingleTestObserver<int>::create();

    requester
        ->requestResponse(Payload(
            RSocketPayloadUtils::buildIOBufFromString(data_sizes, niceLongData),
            RSocketPayloadUtils::buildIOBufFromString(
                meta_sizes, niceLongMeta)))
        ->map([&](Payload p) {
          RSocketPayloadUtils::checkSameStrings(
              p.data, niceLongData, "data (received on client)");
          RSocketPayloadUtils::checkSameStrings(
              p.metadata, niceLongMeta, "metadata (received on client)");
          return 0;
        })
        ->subscribe(to);
    to->awaitTerminalEvent();
    to->assertSuccess();
  };

  // All in one big chunk
  checkForSizePattern({}, {});

  // Small chunk, big chunk, small chunk
  checkForSizePattern(
      {100, 10 * 1024 * 1024, 100}, {100, 10 * 1024 * 1024, 100});
}

TEST(RequestResponseTest, MultiSubscribe) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<GenericRequestResponseHandler>(
      [](StringPair const& request) {
        return payload_response(
            "Hello, " + request.first + " " + request.second + "!", ":)");
      }));

  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto to = SingleTestObserver<StringPair>::create();
  auto single = requester->requestResponse(Payload("Jane", "Doe"))
                    ->map(payload_to_stringpair);

  // Subscribe once
  single->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue({"Hello, Jane Doe!", ":)"});

  // Subscribe twice
  to = SingleTestObserver<StringPair>::create();
  single->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue({"Hello, Jane Doe!", ":)"});
}
