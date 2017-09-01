// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include <random>
#include <thread>

#include "RSocketTests.h"
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

#include "yarpl/test_utils/Mocks.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

using namespace yarpl::mocks;
using namespace ::testing;

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

class RequestAfterCancelTestHandler : public rsocket::RSocketResponder {
 public:
  struct Batons {
    folly::Baton<> serverFinished;
    folly::Baton<> clientFinished;
  };

 private:
  Batons& batons_;
  Sequence& subscription_seq_;

 public:
  RequestAfterCancelTestHandler(Batons& batons, Sequence& subscription_seq)
      : batons_(batons), subscription_seq_(subscription_seq) {}

  Reference<Flowable<Payload>> handleRequestStream(Payload p, StreamId)
      override {
    EXPECT_EQ(p.moveDataToString(), "initial");
    return Flowables::fromPublisher<Payload>(
        [this](Reference<flowable::Subscriber<Payload>> subscriber) {
          auto subscription = make_ref<StrictMock<MockSubscription>>();

          // checked once the subscription is destroyed
          EXPECT_CALL(*subscription, request_(1))
              .InSequence(this->subscription_seq_)
              .WillOnce(Invoke([=](auto n) {
                LOCKSTEP_DEBUG("SERVER: got request(" << n << ")");
                EXPECT_EQ(n, 1);

                LOCKSTEP_DEBUG("SERVER: sending onNext('foo')");
                subscriber->onNext(Payload("foo"));
              }));

          EXPECT_CALL(*subscription, cancel_())
              .InSequence(this->subscription_seq_)
              .WillOnce(Invoke([=] {
                LOCKSTEP_DEBUG("SERVER: received cancel()");
                subscriber->onComplete();
                this->batons_.serverFinished.post();
              }));

          // should not recieve another request after the 'cancel'

          LOCKSTEP_DEBUG("SERVER: sending onSubscribe()");
          subscriber->onSubscribe(subscription);
        });
  }
};

TEST(RequestStreamTest, RequestAfterCancel) {
  Sequence server_seq;
  Sequence client_seq;
  RequestAfterCancelTestHandler::Batons batons;

  folly::ScopedEventBaseThread worker;
  auto server = makeServer(
      std::make_shared<RequestAfterCancelTestHandler>(batons, server_seq));
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto subscriber_mock =
      make_ref<testing::StrictMock<yarpl::mocks::MockSubscriber<std::string>>>(
          0);

  Reference<Subscription> subscription;
  EXPECT_CALL(*subscriber_mock, onSubscribe_(_))
      .InSequence(client_seq)
      .WillOnce(Invoke([&](auto s) {
        LOCKSTEP_DEBUG("CLIENT: got onSubscribe(), sending request(1)");
        EXPECT_NE(s, nullptr);
        subscription = s;
        subscription->request(1);
      }));
  EXPECT_CALL(*subscriber_mock, onNext_("foo"))
      .InSequence(client_seq)
      .WillRepeatedly(Invoke([&](auto) {
        EXPECT_NE(subscription, nullptr);
        LOCKSTEP_DEBUG(
            "CLIENT: got onNext(foo), sending cancel() then request(1)");
        subscription->cancel();
        subscription->request(1); // should just be a no-op
      }));
  EXPECT_CALL(*subscriber_mock, onComplete_())
      .InSequence(client_seq)
      .WillOnce(Invoke([&]() {
        LOCKSTEP_DEBUG("CLIENT: got onComplete()");
        batons.clientFinished.post();
      }));

  LOCKSTEP_DEBUG("RUNNER: doing requestStream()");
  requester->requestStream(Payload("initial"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(subscriber_mock);

  CHECK_WAIT(batons.clientFinished);
  CHECK_WAIT(batons.serverFinished);
  LOCKSTEP_DEBUG("RUNNER: finished!");
}
