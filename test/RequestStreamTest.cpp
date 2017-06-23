// Copyright 2004-present Facebook. All Rights Reserved.

#include <thread>

#include "RSocketTests.h"
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

namespace {
class TestHandlerSync : public rsocket::RSocketResponder {
 public:
  Reference<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId) override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowables::range(1, 10)->map([name = std::move(requestString)](
        int64_t v) {
      std::stringstream ss;
      ss << "Hello " << name << " " << v << "!";
      std::string s = ss.str();
      return Payload(s, "metadata");
    });
  }
};

TEST(RequestStreamTest, HelloSync) {
  auto server = makeServer(std::make_shared<TestHandlerSync>());
  auto client = makeClient(*server->listeningPort());
  auto requester = client->connect().get();
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

class TestHandlerAsync : public rsocket::RSocketResponder {
 public:
  Reference<Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId) override {
    // string from payload data
    auto requestString = request.moveDataToString();

    return Flowables::fromPublisher<
        Payload>([requestString = std::move(requestString)](
        Reference<flowable::Subscriber<Payload>> subscriber) {
      std::thread([
        requestString = std::move(requestString),
        subscriber = std::move(subscriber)
      ]() {
        Flowables::range(1, 40)
            ->map([name = std::move(requestString)](int64_t v) {
              std::stringstream ss;
              ss << "Hello " << name << " " << v << "!";
              std::string s = ss.str();
              return Payload(s, "metadata");
            })
            ->subscribe(subscriber);
      }).detach();
    });
  }
};
}

TEST(RequestStreamTest, HelloAsync) {
  auto server = makeServer(std::make_shared<TestHandlerAsync>());
  auto client = makeClient(*server->listeningPort());
  auto requester = client->connect().get();
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
