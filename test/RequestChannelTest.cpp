// Copyright 2004-present Facebook. All Rights Reserved.

#include <thread>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>

#include "RSocketTests.h"
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
  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      yarpl::Reference<Flowable<rsocket::Payload>> request,
      rsocket::StreamId) override {
    // say "Hello" to each name on the input stream
    return request->map([initialPayload = std::move(initialPayload)](
        Payload p) {
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
          Flowables::justN({"/hello", "Bob", "Jane"})->map([](std::string v) {
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

TEST(RequestChannelTest, RequestOnDisconnectedClient) {
  folly::ScopedEventBaseThread worker;
  auto client = makeDisconnectedClient(worker.getEventBase());
  auto requester = client->getRequester();

  bool did_call_on_error = false;
  folly::Baton<> wait_for_on_error;

  auto instream = Flowables::empty<Payload>();
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
  ASSERT(did_call_on_error);
}

class ResponderLongStream : public rsocket::RSocketResponder {
 public:
  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload,
      yarpl::Reference<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    auto ts = TestSubscriber<std::string>::create();
    requestStream->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(ts);

    // output 1 - 100
    return Flowables::range(1, 100)->map([](int64_t v) {
      std::stringstream ss;
      ss << "Server stream: " << v;
      std::string s = ss.str();
      return Payload(s, "metadata");
    });
  }
};


class ResponderShortStream : public rsocket::RSocketResponder {
 public:
  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload,
      yarpl::Reference<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    auto ts = TestSubscriber<std::string>::create();
    requestStream->map([](auto p) { return p.moveDataToString(); })
                 ->subscribe(ts);

    return Flowable<Payload>::create(
        [&](Reference<Subscriber<Payload>> subscriber, int64_t) {
          subscriber->onNext(Payload("some data", "meta"));
          subscriber->onNext(Payload("more data", "meta"));
          subscriber->onComplete();
          return std::make_tuple(int64_t(2), true);
        });
  }
};

TEST(RequestChannelTest, CompleteRequesterResponderContinues) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<ResponderLongStream>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto ts = TestSubscriber<std::string>::create(5);

  folly::Baton<> wait_for_on_complete;

  auto shortFlowable = Flowable<Payload>::create(
      [&](Reference<Subscriber<Payload>> subscriber, int64_t) {
        subscriber->onNext(Payload("some data", "meta"));
        subscriber->onNext(Payload("more data", "meta"));
        subscriber->onComplete();
        wait_for_on_complete.post();
        return std::make_tuple(int64_t(2), true);
      });

  requester->requestChannel(shortFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);

  // client stream closes before Responder can finish
  wait_for_on_complete.timed_wait(std::chrono::milliseconds(100));

  ts->request(5);

  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(100);
  ts->assertValueAt(0, "Server stream: 1");
  ts->assertValueAt(99, "Server stream: 100");
}


TEST(RequestChannelTest, CompleteResponderRequesterContinues) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<ResponderLongStream>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto ts = TestSubscriber<std::string>::create();

  folly::Baton<> wait_for_on_complete;

  auto longFlowable = Flowables::range(1, 100)->map([](int64_t v) {
    std::stringstream ss;
    ss << "Client stream: " << v;
    std::string s = ss.str();
    return Payload(s, "metadata");
  });

  requester->requestChannel(longFlowable)
           ->map([](auto p) { return p.moveDataToString(); })
           ->subscribe(ts);

  // server stream closes before Requester can finish
  
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(100);
  ts->assertValueAt(0, "Server stream: 1");
  ts->assertValueAt(99, "Server stream: 100");
}

// Sandbox REMOVE BEFORE MAKING A PULL REQUEST

class TestHandler2Way : public rsocket::RSocketResponder {
 public:
  /// Handles a new inbound Stream requested by the other end.
  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      yarpl::Reference<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    auto ts = TestSubscriber<std::string>::create();
    requestStream
        ->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(ts);
    LOG(INFO) << initialPayload;
    auto initialPayloadString = initialPayload.moveDataToString();
    // say "Hello" to each name on the input stream
    return Flowables::range(1, 10)->map([name = std::move(initialPayloadString)](
        int64_t v) {
      std::stringstream ss;
      ss << "Hello " << name << " " << v << "!";
      std::string s = ss.str();
      return Payload(s, "metadata");
    });
  }
};


TEST(RequestChannelTest, ManualChannel) {
  LOG(INFO) << "ManualChannel";
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<TestHandler2Way>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto ts = TestSubscriber<std::string>::create();

  auto flow = Flowable<Payload>::create([](
      Reference<Subscriber<Payload>> subscriber, int64_t) {
    subscriber->onNext(Payload("1", "meta"));
    subscriber->onNext(Payload("2", "meta"));
    subscriber->onNext(Payload("3", "meta"));
    subscriber->onNext(Payload("4", "meta"));
    return std::make_tuple(int64_t(1), true);
  });

  requester
      ->requestChannel(
          flow)
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

auto flow = Flowable<Payload>::create([](
        Reference<Subscriber<Payload>> subscriber, int64_t) {
      subscriber->onNext(Payload("1", "meta"));
      subscriber->onNext(Payload("2", "meta"));
      subscriber->onNext(Payload("3", "meta"));
      subscriber->onNext(Payload("4", "meta"));
      return std::make_tuple(int64_t(1), true);
    });


// TODO complete from requester, responder continues
// TODO complete from responder, requester continues
// TODO cancel from requester, shuts down
// TODO flow control from requester to responder
// TODO flow control from responder to requester
// TODO failure on responder, requester sees
// TODO failure on request, requester sees
// TODO failure from requester ... what happens?
