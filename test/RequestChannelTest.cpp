// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include <thread>

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

class TestChannelResponder : public rsocket::RSocketResponder {
 public:
  TestChannelResponder(
      int64_t rangeEnd = 10,
      int64_t initialSubReq = credits::kNoFlowControl)
      : rangeEnd_{rangeEnd},
        testSubscriber_{TestSubscriber<std::string>::create(initialSubReq)} {}

  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      yarpl::Reference<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    // add initial payload to testSubscriber values list
    testSubscriber_->onNext(initialPayload.moveDataToString());

    requestStream->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(testSubscriber_);

    return Flowables::range(1, rangeEnd_)->map([&](int64_t v) {
      std::stringstream ss;
      ss << "Responder stream: " << v << " of " << rangeEnd_;
      std::string s = ss.str();
      return Payload(s, "metadata");
    });
  }

  Reference<TestSubscriber<std::string>> getChannelSubscriber() {
    return testSubscriber_;
  }

 private:
  int64_t rangeEnd_;
  Reference<TestSubscriber<std::string>> testSubscriber_;
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
      Flowables::range(1, requesterRangeEnd)->map([&](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  // finish streaming from Requester
  responderSubscriber->awaitTerminalEvent();
  responderSubscriber->assertSuccess();
  responderSubscriber->assertValueCount(10);
  responderSubscriber->assertValueAt(0, "Requester stream: 1 of 10");
  responderSubscriber->assertValueAt(9, "Requester stream: 10 of 10");

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
      Flowables::range(1, requesterRangeEnd)->map([&](int64_t v) {
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
  int64_t responderRange = 10;
  int64_t responderSubscriberInitialRequest = 0;

  auto responder = std::make_shared<TestChannelResponder>(
      responderRange, responderSubscriberInitialRequest);

  folly::ScopedEventBaseThread worker;
  auto server = makeServer(responder);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto requestSubscriber = TestSubscriber<std::string>::create(1);
  auto responderSubscriber = responder->getChannelSubscriber();

  int64_t requesterRangeEnd = 10;

  auto requesterFlowable =
      Flowables::range(1, requesterRangeEnd)->map([&](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  responderSubscriber->awaitValueCount(1);
  requestSubscriber->awaitValueCount(1);

  for (int i = 2; i <= 10; i++) {
    requestSubscriber->request(1);
    responderSubscriber->request(1);

    responderSubscriber->awaitValueCount(i);
    requestSubscriber->awaitValueCount(i);

    requestSubscriber->assertValueCount(i);
    responderSubscriber->assertValueCount(i);
  }

  requestSubscriber->awaitTerminalEvent();
  responderSubscriber->awaitTerminalEvent();

  requestSubscriber->assertSuccess();
  responderSubscriber->assertSuccess();

  requestSubscriber->assertValueAt(0, "Responder stream: 1 of 10");
  requestSubscriber->assertValueAt(9, "Responder stream: 10 of 10");

  responderSubscriber->assertValueAt(0, "Requester stream: 1 of 10");
  responderSubscriber->assertValueAt(9, "Requester stream: 10 of 10");
}

TEST(RequestChannelTest, DISABLED_CancelFromRequester) {
  int64_t responderRange = 100;
  int64_t responderSubscriberInitialRequest = 100;

  auto responder = std::make_shared<TestChannelResponder>(
      responderRange, responderSubscriberInitialRequest);

  folly::ScopedEventBaseThread worker;
  auto server = makeServer(responder);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto requestSubscriber = TestSubscriber<std::string>::create(100);
  auto responderSubscriber = responder->getChannelSubscriber();

  int64_t requesterRangeEnd = 100;

  auto requesterFlowable =
      Flowables::range(1, requesterRangeEnd)->map([&](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  requestSubscriber->awaitValueCount(20);
  requestSubscriber->cancel();

  responderSubscriber->awaitTerminalEvent();

  // Responder Subscriber should be at 100
  // Requester Subscriber should be 20 - 30ish, depending on timing
  LOG(INFO) << "Responder Subscriber: " << responderSubscriber->getValueCount();
  LOG(INFO) << "Requester Subscriber: " << requestSubscriber->getValueCount();

  // This crashes with a memory error (NULL PTR), I don't think it should?

}

class TestChannelResponderFailure : public rsocket::RSocketResponder {
 public:
  TestChannelResponderFailure()
      : testSubscriber_{TestSubscriber<std::string>::create()} {}

  yarpl::Reference<Flowable<rsocket::Payload>> handleRequestChannel(
      rsocket::Payload initialPayload,
      yarpl::Reference<Flowable<rsocket::Payload>> requestStream,
      rsocket::StreamId) override {
    // add initial payload to testSubscriber values list
    testSubscriber_->onNext(initialPayload.moveDataToString());

    requestStream->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(testSubscriber_);

    return Flowables::error<Payload>(
        std::runtime_error("A wild Error appeared!"));
  }

  Reference<TestSubscriber<std::string>> getChannelSubscriber() {
    return testSubscriber_;
  }

 private:
  Reference<TestSubscriber<std::string>> testSubscriber_;
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
      Flowables::range(1, requesterRangeEnd)->map([&](int64_t v) {
        std::stringstream ss;
        ss << "Requester stream: " << v << " of " << requesterRangeEnd;
        std::string s = ss.str();
        return Payload(s, "metadata");
      });

  requester->requestChannel(requesterFlowable)
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(requestSubscriber);

  // failure streaming from Responder
  requestSubscriber->awaitTerminalEvent();
  requestSubscriber->assertOnErrorMessage("A wild Error appeared!");

  responderSubscriber->awaitTerminalEvent();
  responderSubscriber->assertValueAt(0, "Requester stream: 1 of 10");
  responderSubscriber->assertValueAt(9, "Requester stream: 10 of 10");
}

TEST(RequestChannelTest, FailureOnRequestRequesterSees) {
  // ???
}

TEST(RequestChannelTest, FailureFromRequester) {
  // ???
}
