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
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

using namespace yarpl::flowable;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

namespace {
class TestHandlerSync : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId) override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowable<>::range(1, 10)->map(
        [name = std::move(requestString)](int64_t v) {
          std::stringstream ss;
          ss << "Hello " << name << " " << v << "!";
          std::string s = ss.str();
          return Payload(s, "metadata");
        });
  }
};

TEST(RequestStreamTest, HelloSync) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerSync>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create();
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);
  ts->assertValueAt(0, "Hello Bob 1!");
  ts->assertValueAt(9, "Hello Bob 10!");
}

TEST(RequestStreamTest, HelloFlowControl) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerSync>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(5);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);

  ts->awaitValueCount(5);

  ts->assertValueCount(5);
  ts->assertValueAt(0, "Hello Bob 1!");
  ts->assertValueAt(4, "Hello Bob 5!");

  ts->request(5);

  ts->awaitValueCount(10);

  ts->assertValueCount(10);
  ts->assertValueAt(5, "Hello Bob 6!");
  ts->assertValueAt(9, "Hello Bob 10!");

  ts->awaitTerminalEvent();
  ts->assertSuccess();
}

TEST(RequestStreamTest, HelloNoFlowControl) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerSync>());
  auto stats = std::make_shared<RSocketStatsFlowControl>();
  auto client = makeClient(
      worker.getEventBase(), *server->listeningPort(), nullptr, stats);
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(1000);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);
  ts->assertValueAt(0, "Hello Bob 1!");
  ts->assertValueAt(9, "Hello Bob 10!");

  // Make sure that the initial requestN in the Stream Request Frame
  // is already enough and no other requestN messages are sent.
  EXPECT_EQ(stats->writeRequestN_, 0);
}

class TestHandlerAsync : public rsocket::RSocketResponder {
 public:
  explicit TestHandlerAsync(folly::Executor& executor) : executor_(executor) {}

  std::shared_ptr<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId) override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowable<>::range(1, 40)
        ->map([name = std::move(requestString)](int64_t v) {
          std::stringstream ss;
          ss << "Hello " << name << " " << v << "!";
          std::string s = ss.str();
          return Payload(s, "metadata");
        })
        ->subscribeOn(executor_);
  }

 private:
  folly::Executor& executor_;
};
} // namespace

TEST(RequestStreamTest, HelloAsync) {
  folly::ScopedEventBaseThread worker;
  folly::ScopedEventBaseThread worker2;
  auto server =
      makeServer(std::make_shared<TestHandlerAsync>(*worker2.getEventBase()));
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create();
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(40);
  ts->assertValueAt(0, "Hello Bob 1!");
  ts->assertValueAt(39, "Hello Bob 40!");
}

TEST(RequestStreamTest, RequestOnDisconnectedClient) {
  folly::ScopedEventBaseThread worker;
  auto client = makeDisconnectedClient(worker.getEventBase());
  auto requester = client->getRequester();

  bool did_call_on_error = false;
  folly::Baton<> wait_for_on_error;

  requester->requestStream(Payload("foo", "bar"))
      ->subscribe(
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

class TestHandlerResponder : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload, StreamId)
      override {
    return Flowable<Payload>::error(
        std::runtime_error("A wild Error appeared!"));
  }
};

TEST(RequestStreamTest, HandleError) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerResponder>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create();
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  ts->awaitTerminalEvent();
  // Hide the user error from the logs
  ts->assertOnErrorMessage("ErrorWithPayload");
  EXPECT_TRUE(ts->getException().with_exception([](ErrorWithPayload& err) {
    EXPECT_STREQ(
        "A wild Error appeared!", err.payload.moveDataToString().c_str());
  }));
}

class TestErrorAfterOnNextResponder : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId) override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowable<Payload>::create(
        [name = std::move(requestString)](
            Subscriber<Payload>& subscriber, int64_t requested) {
          EXPECT_GT(requested, 1);
          subscriber.onNext(Payload(name, "meta"));
          subscriber.onNext(Payload(name, "meta"));
          subscriber.onNext(Payload(name, "meta"));
          subscriber.onNext(Payload(name, "meta"));
          subscriber.onError(std::runtime_error("A wild Error appeared!"));
        });
  }
};

TEST(RequestStreamTest, HandleErrorMidStream) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestErrorAfterOnNextResponder>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create();
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertValueCount(4);
  ts->assertOnErrorMessage("ErrorWithPayload");
  EXPECT_TRUE(ts->getException().with_exception([](ErrorWithPayload& err) {
    EXPECT_STREQ(
        "A wild Error appeared!", err.payload.moveDataToString().c_str());
  }));
}

struct LargePayloadStreamHandler : public rsocket::RSocketResponder {
  LargePayloadStreamHandler(
      std::string const& data,
      std::string const& meta,
      Payload const& seedPayload)
      : data(data), meta(meta), seedPayload(seedPayload) {}

  std::shared_ptr<yarpl::flowable::Flowable<Payload>> handleRequestStream(
      Payload initialPayload,
      StreamId) override {
    RSocketPayloadUtils::checkSameStrings(
        initialPayload.data, data, "data received in initial payload");
    RSocketPayloadUtils::checkSameStrings(
        initialPayload.metadata, meta, "metadata received in initial payload");

    return yarpl::flowable::Flowable<Payload>::create([&](auto& subscriber,
                                                          int64_t num) {
             while (num--) {
               auto p = Payload(
                   seedPayload.data->clone(), seedPayload.metadata->clone());
               subscriber.onNext(std::move(p));
             }
           })
        ->take(3);
  }

  std::string const& data;
  std::string const& meta;
  Payload const& seedPayload;
};

TEST(RequestStreamTest, TestLargePayload) {
  LOG(INFO) << "Building up large data/metadata, this may take a moment...";
  // ~20 megabytes per frame (metadata + data)
  std::string const niceLongData = RSocketPayloadUtils::makeLongString(
      RSocketPayloadUtils::LargeRequestSize, "ABCDEFGH");
  std::string const niceLongMeta = RSocketPayloadUtils::makeLongString(
      RSocketPayloadUtils::LargeRequestSize, "12345678");

  LOG(INFO) << "Built meta size: " << niceLongMeta.size()
            << " data size: " << niceLongData.size();

  auto checkForSizePattern = [&](std::vector<size_t> const& meta_sizes,
                                 std::vector<size_t> const& data_sizes) {
    folly::ScopedEventBaseThread worker;
    auto seedPayload = Payload(
        RSocketPayloadUtils::buildIOBufFromString(data_sizes, niceLongData),
        RSocketPayloadUtils::buildIOBufFromString(meta_sizes, niceLongMeta));
    auto makePayload = [&] {
      return Payload(seedPayload.data->clone(), seedPayload.metadata->clone());
    };

    auto handler = std::make_shared<LargePayloadStreamHandler>(
        niceLongData, niceLongMeta, seedPayload);
    auto server = makeServer(handler);

    auto client = makeClient(worker.getEventBase(), *server->listeningPort());
    auto requester = client->getRequester();

    auto to = TestSubscriber<int>::create();

    requester->requestStream(makePayload())
        ->map([&](Payload p) {
          RSocketPayloadUtils::checkSameStrings(
              p.data, niceLongData, "data received on client");
          RSocketPayloadUtils::checkSameStrings(
              p.metadata, niceLongMeta, "metadata received on client");
          return 0;
        })
        ->subscribe(to);
    to->awaitTerminalEvent(std::chrono::seconds{20});
    to->assertValueCount(3);
    to->assertSuccess();
  };

  // All in one big chunk
  checkForSizePattern({}, {});

  // Small chunk, big chunk, small chunk
  checkForSizePattern({100, 5 * 1024 * 1024, 100}, {100, 5 * 1024 * 1024, 100});
}

TEST(RequestStreamTest, MultiSubscribe) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerSync>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create();
  auto stream = requester->requestStream(Payload("Bob"))->map([](auto p) {
    return p.moveDataToString();
  });

  // First subscribe
  stream->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);
  ts->assertValueAt(0, "Hello Bob 1!");
  ts->assertValueAt(9, "Hello Bob 10!");

  // Second subscribe
  ts = TestSubscriber<std::string>::create();
  stream->subscribe(ts);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);
  ts->assertValueAt(0, "Hello Bob 1!");
  ts->assertValueAt(9, "Hello Bob 10!");
}
