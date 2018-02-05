// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include <thread>

#include "RSocketTests.h"
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

#include "rs/map.h"
#include "rs/pipe.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

namespace {
class TestHandlerSync : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload request, StreamId)
      override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowable<>::range(1, 10)->map([name = std::move(requestString)](
        int64_t v) {
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
  auto ts = TestSubscriber<std::string>::create();
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
  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload request, StreamId)
      override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowable<Payload>::fromPublisher(
        [requestString = std::move(requestString)](
            std::shared_ptr<flowable::Subscriber<Payload>> subscriber) {
          std::thread([requestString = std::move(requestString),
                       subscriber = std::move(subscriber)]() {
            Flowable<>::range(1, 40)
                ->map([name = std::move(requestString)](int64_t v) {
                  std::stringstream ss;
                  ss << "Hello " << name << " " << v << "!";
                  std::string s = ss.str();
                  return Payload(s, "metadata");
                })
                ->subscribe(subscriber);
          })
              .detach();
        });
  }
};
} // namespace

TEST(RequestStreamTest, HelloAsync) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerAsync>());
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

  CHECK_WAIT(wait_for_on_error);
  ASSERT(did_call_on_error);
}

class TestHandlerResponder : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload, StreamId) override {
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
  ts->assertOnErrorMessage("A wild Error appeared!");
}

class TestErrorAfterOnNextResponder : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload request, StreamId)
      override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowable<Payload>::create([name = std::move(requestString)](
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
  ts->assertOnErrorMessage("A wild Error appeared!");
}

TEST(RequestStreamTest, HelloAsyncRs) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandlerAsync>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto pipe = shk::Pipe(
      requester->rsRequestStream([] { return Payload("Bob"); }),
      shk::Map([](auto p) { return p.moveDataToString(); }));

  folly::Baton<> baton;
  int got{0};

  auto sub = pipe.Subscribe(shk::MakeSubscriber(
      [&](auto str) {
        got++;
        if (got == 1) {
          CHECK_EQ(str, "Hello Bob 1!");
        }
      },
      [&](auto err) {
        // err
        try {
          std::rethrow_exception(err);
        } catch (const std::exception& e) {
          FAIL() << "failed with " << e.what();
        }
      },
      [&] { baton.post(); }));
  sub.Request(shk::ElementCount{100});
  CHECK(baton.try_wait_for(std::chrono::seconds{1}));
  CHECK_EQ(got, 40);
}