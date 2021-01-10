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
#include <gtest/gtest.h>
#include <thread>

#include "RSocketTests.h"
#include "rsocket/test/test_utils/GenericRequestResponseHandler.h"
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

/**
 * Test a finite stream both directions.
 */
class TestHandlerHello : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestChannel(
      rsocket::Payload initialPayload,
      std::shared_ptr<yarpl::flowable::Flowable<rsocket::Payload>> stream,
      rsocket::StreamId /*streamId*/) override {
    // say "Hello" to each name on the input stream
    return stream->map([initialPayload = std::move(initialPayload)](Payload p) {
      std::stringstream ss;
      ss << "[" << initialPayload.cloneDataToString() << "] "
         << "Hello " << p.moveDataToString() << "!";
      std::string s = ss.str();

      return Payload(s);
    });
  }
};

TEST(RequestChannelTest, Hello) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerHello>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto ts = TestSubscriber<std::string>::create();
  requester
      ->requestChannel(
          Payload("/hello"),
          Flowable<>::justN({"Bob", "Jane"})->map([](std::string v) {
            return Payload(v);
          }))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);

  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(2);
  // assert that we echo back the 2nd and 3rd request values
  // with the 1st initial payload prepended to each
  ts->assertValueAt(0, "[/hello] Hello Bob!");
  ts->assertValueAt(1, "[/hello] Hello Jane!");
}

TEST(RequestChannelTest, HelloNoFlowControl) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerHello>());
  auto stats = std::make_shared<RSocketStatsFlowControl>();
  auto client = makeClient(
      worker.getEventBase(), *server->listeningPort(), nullptr, stats);
  auto requester = client->getRequester();

  auto ts = TestSubscriber<std::string>::create(1000);
  requester
      ->requestChannel(
          Payload("/hello"),
          Flowable<>::justN({"Bob", "Jane"})->map([](std::string v) {
            return Payload(v);
          }))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);

  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(2);
  // assert that we echo back the 2nd and 3rd request values
  // with the 1st initial payload prepended to each
  ts->assertValueAt(0, "[/hello] Hello Bob!");
  ts->assertValueAt(1, "[/hello] Hello Jane!");

  // Make sure that the initial requestN in the Stream Request Frame
  // is already enough and no other requestN messages are sent.
  EXPECT_EQ(stats->writeRequestN_, 0);
}

TEST(RequestChannelTest, RequestOnDisconnectedClient) {
  folly::ScopedEventBaseThread worker;
  auto client = makeDisconnectedClient(worker.getEventBase());
  auto requester = client->getRequester();

  bool did_call_on_error = false;
  folly::Baton<> wait_for_on_error;

  auto instream = Flowable<Payload>::empty();
  requester->requestChannel(instream)->subscribe(
      [](auto /* payload */) {
        // onNext shouldn't be called
        FAIL();
      },
      [&](folly::exception_wrapper) {
        did_call_on_error = true;
        wait_for_on_error.post();
      },
      []() {
        // onComplete shouldn't be called
        FAIL();
      });

  wait_for_on_error.timed_wait(std::chrono::milliseconds(100));
  ASSERT_TRUE(did_call_on_error);
}

class TestChannelResponder : public rsocket::RSocketResponder {
 public:
  TestChannelResponder(
      int64_t rangeEnd = 10,
      int64_t initialSubReq = credits::kNoFlowControl)
      : rangeEnd_{rangeEnd},
        testSubscriber_{TestSubscriber<std::string>::create(initialSubReq)} {}

  std::shared_ptr<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      std::shared_ptr<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    // add initial payload to testSubscriber values list
    testSubscriber_->manuallyPush(initialPayload.moveDataToString());

    requestStream->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(testSubscriber_);

    return Flowable<>::range(1, rangeEnd_)->map([&](int64_t v) {
      std::stringstream ss;
      ss << "Responder stream: " << v << " of " << rangeEnd_;
      std::string s = ss.str();
      return Payload(s, "metadata");
    });
  }

  std::shared_ptr<TestSubscriber<std::string>> getChannelSubscriber() {
    return testSubscriber_;
  }

 private:
  int64_t rangeEnd_;
  std::shared_ptr<TestSubscriber<std::string>> testSubscriber_;
};

TEST(RequestChannelTest, CompleteRequesterResponderContinues) {
  int64_t responderRange = 100;
  int64_t responderSubscriberInitialRequest = credits::kNoFlowControl;

  auto responder = std::make_shared<TestChannelResponder>(
      responderRange, responderSubscriberInitialRequest);
  folly::ScopedEventBaseThread worker;

  auto server = makeServer(responder);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto requestSubscriber = TestSubscriber<std::string>::create(50);
  auto responderSubscriber = responder->getChannelSubscriber();

  int64_t requesterRangeEnd = 10;

  auto requesterFlowable =
      Flowable<>::range(1, requesterRangeEnd)->map([=](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(Payload("Initial Request"), requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  // finish streaming from Requester
  responderSubscriber->awaitTerminalEvent();
  responderSubscriber->assertSuccess();
  responderSubscriber->assertValueCount(11);
  responderSubscriber->assertValueAt(0, "Initial Request");
  responderSubscriber->assertValueAt(1, "Requester stream: 1 of 10");
  responderSubscriber->assertValueAt(10, "Requester stream: 10 of 10");

  // Requester stream is closed, Responder continues
  requestSubscriber->request(50);
  requestSubscriber->awaitTerminalEvent();
  requestSubscriber->assertSuccess();
  requestSubscriber->assertValueCount(100);
  requestSubscriber->assertValueAt(0, "Responder stream: 1 of 100");
  requestSubscriber->assertValueAt(99, "Responder stream: 100 of 100");
}

TEST(RequestChannelTest, CompleteResponderRequesterContinues) {
  int64_t responderRange = 10;
  int64_t responderSubscriberInitialRequest = 50;

  auto responder = std::make_shared<TestChannelResponder>(
      responderRange, responderSubscriberInitialRequest);

  folly::ScopedEventBaseThread worker;
  auto server = makeServer(responder);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto requestSubscriber = TestSubscriber<std::string>::create();
  auto responderSubscriber = responder->getChannelSubscriber();

  int64_t requesterRangeEnd = 100;

  auto requesterFlowable =
      Flowable<>::range(1, requesterRangeEnd)->map([=](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  // finish streaming from Responder
  requestSubscriber->awaitTerminalEvent();
  requestSubscriber->assertSuccess();
  requestSubscriber->assertValueCount(10);
  requestSubscriber->assertValueAt(0, "Responder stream: 1 of 10");
  requestSubscriber->assertValueAt(9, "Responder stream: 10 of 10");

  // Responder stream is closed, Requester continues
  responderSubscriber->request(50);
  responderSubscriber->awaitTerminalEvent();
  responderSubscriber->assertSuccess();
  responderSubscriber->assertValueCount(100);
  responderSubscriber->assertValueAt(0, "Requester stream: 1 of 100");
  responderSubscriber->assertValueAt(99, "Requester stream: 100 of 100");
}

TEST(RequestChannelTest, FlowControl) {
  constexpr int64_t responderRange = 10;
  constexpr int64_t responderSubscriberInitialRequest = 0;

  auto responder = std::make_shared<TestChannelResponder>(
      responderRange, responderSubscriberInitialRequest);

  folly::ScopedEventBaseThread worker;
  auto server = makeServer(responder);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto requestSubscriber = TestSubscriber<std::string>::create(0);
  auto responderSubscriber = responder->getChannelSubscriber();

  constexpr int64_t requesterRangeEnd = 10;

  auto requesterFlowable =
      Flowable<>::range(1, requesterRangeEnd)->map([&](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(Payload("Initial Request"), requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  // Wait till the Channel is created
  responderSubscriber->awaitValueCount(1);

  for (int i = 1; i <= 10; i++) {
    requestSubscriber->request(1);
    requestSubscriber->awaitValueCount(i);
    requestSubscriber->assertValueCount(i);
  }

  for (int i = 1; i <= 10; i++) {
    responderSubscriber->request(1);
    // the channel initial payload was pushed to responderSubscriber so we
    // need to add this one item to expected
    responderSubscriber->awaitValueCount(i + 1);
    responderSubscriber->assertValueCount(i + 1);
  }

  requestSubscriber->awaitTerminalEvent();
  responderSubscriber->awaitTerminalEvent();

  requestSubscriber->assertSuccess();
  responderSubscriber->assertSuccess();

  requestSubscriber->assertValueAt(0, "Responder stream: 1 of 10");
  requestSubscriber->assertValueAt(9, "Responder stream: 10 of 10");

  responderSubscriber->assertValueAt(0, "Initial Request");
  responderSubscriber->assertValueAt(1, "Requester stream: 1 of 10");
  responderSubscriber->assertValueAt(10, "Requester stream: 10 of 10");
}

class TestChannelResponderFailure : public rsocket::RSocketResponder {
 public:
  TestChannelResponderFailure()
      : testSubscriber_{TestSubscriber<std::string>::create()} {}

  std::shared_ptr<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      std::shared_ptr<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    // add initial payload to testSubscriber values list
    testSubscriber_->manuallyPush(initialPayload.moveDataToString());

    requestStream->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(testSubscriber_);

    return Flowable<Payload>::error(
        std::runtime_error("A wild Error appeared!"));
  }

  std::shared_ptr<TestSubscriber<std::string>> getChannelSubscriber() {
    return testSubscriber_;
  }

 private:
  std::shared_ptr<TestSubscriber<std::string>> testSubscriber_;
};

TEST(RequestChannelTest, FailureOnResponderRequesterSees) {
  auto responder = std::make_shared<TestChannelResponderFailure>();

  folly::ScopedEventBaseThread worker;
  auto server = makeServer(responder);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto requestSubscriber = TestSubscriber<std::string>::create();
  auto responderSubscriber = responder->getChannelSubscriber();

  int64_t requesterRangeEnd = 10;

  auto requesterFlowable =
      Flowable<>::range(1, requesterRangeEnd)->map([&](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(Payload("Initial Request"), requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  // failure streaming from Responder
  requestSubscriber->awaitTerminalEvent();
  requestSubscriber->assertOnErrorMessage("ErrorWithPayload");
  EXPECT_TRUE(requestSubscriber->getException().with_exception(
      [](ErrorWithPayload& err) {
        EXPECT_STREQ(
            "A wild Error appeared!", err.payload.moveDataToString().c_str());
      }));

  responderSubscriber->awaitTerminalEvent();
  responderSubscriber->assertSuccess();
  responderSubscriber->assertValueCount(1);
  responderSubscriber->assertValueAt(0, "Initial Request");
}

struct LargePayloadChannelHandler : public rsocket::RSocketResponder {
  LargePayloadChannelHandler(std::string const& data, std::string const& meta)
      : data(data), meta(meta) {}

  std::shared_ptr<yarpl::flowable::Flowable<Payload>> handleRequestChannel(
      Payload initialPayload,
      std::shared_ptr<yarpl::flowable::Flowable<Payload>> stream,
      StreamId) override {
    RSocketPayloadUtils::checkSameStrings(
        initialPayload.data, data, "data received in initial payload");
    RSocketPayloadUtils::checkSameStrings(
        initialPayload.metadata, meta, "metadata received in initial payload");

    return stream->map([&](Payload payload) {
      RSocketPayloadUtils::checkSameStrings(
          payload.data, data, "data received in server stream");
      RSocketPayloadUtils::checkSameStrings(
          payload.metadata, meta, "metadata received in server stream");
      return payload;
    });
  }

  std::string const& data;
  std::string const& meta;
};

TEST(RequestChannelTest, TestLargePayload) {
  LOG(INFO) << "Building up large data/metadata, this may take a moment...";
  std::string const niceLongData = RSocketPayloadUtils::makeLongString(
      RSocketPayloadUtils::LargeRequestSize, "ABCDEFGH");
  std::string const niceLongMeta = RSocketPayloadUtils::makeLongString(
      RSocketPayloadUtils::LargeRequestSize, "12345678");

  LOG(INFO) << "Built meta size: " << niceLongMeta.size()
            << " data size: " << niceLongData.size();

  folly::ScopedEventBaseThread worker;
  auto handler =
      std::make_shared<LargePayloadChannelHandler>(niceLongData, niceLongMeta);
  auto server = makeServer(handler);

  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto checkForSizePattern = [&](std::vector<size_t> const& meta_sizes,
                                 std::vector<size_t> const& data_sizes) {
    auto to = TestSubscriber<int>::create();

    auto seedPayload = Payload(
        RSocketPayloadUtils::buildIOBufFromString(data_sizes, niceLongData),
        RSocketPayloadUtils::buildIOBufFromString(meta_sizes, niceLongMeta));

    auto makePayload = [&] {
      return Payload(seedPayload.data->clone(), seedPayload.metadata->clone());
    };

    auto requests =
        yarpl::flowable::Flowable<Payload>::create([&](auto& subscriber,
                                                       int64_t num) {
          while (num--) {
            subscriber.onNext(makePayload());
          }
        })->take(3);

    requester->requestChannel(std::move(requests))
        ->map([&](Payload p) {
          RSocketPayloadUtils::checkSameStrings(
              p.data, niceLongData, "data received on client");
          RSocketPayloadUtils::checkSameStrings(
              p.metadata, niceLongMeta, "metadata received on client");
          return 0;
        })
        ->subscribe(to);
    to->awaitTerminalEvent(std::chrono::seconds{20});
    to->assertValueCount(2);
    to->assertSuccess();
  };

  // All in one big chunk
  checkForSizePattern({}, {});

  // Small chunk, big chunk, small chunk
  checkForSizePattern({100, 5 * 1024 * 1024, 100}, {100, 5 * 1024 * 1024, 100});
}

TEST(RequestChannelTest, MultiSubscribe) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerHello>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto ts = TestSubscriber<std::string>::create();
  auto stream =
      requester
          ->requestChannel(
              Payload("/hello"),
              Flowable<>::justN({"Bob", "Jane"})->map([](std::string v) {
                return Payload(v);
              }))
          ->map([](auto p) { return p.moveDataToString(); });

  // First subscribe
  stream->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(2);
  // assert that we echo back the 2nd and 3rd request values
  // with the 1st initial payload prepended to each
  ts->assertValueAt(0, "[/hello] Hello Bob!");
  ts->assertValueAt(1, "[/hello] Hello Jane!");

  // Second subscribe
  ts = TestSubscriber<std::string>::create();
  stream->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(2);
  // assert that we echo back the 2nd and 3rd request values
  // with the 1st initial payload prepended to each
  ts->assertValueAt(0, "[/hello] Hello Bob!");
  ts->assertValueAt(1, "[/hello] Hello Jane!");
}
