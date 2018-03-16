// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include <iostream>
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

struct LockstepBatons {
  folly::Baton<> onSecondPayloadSent;
  folly::Baton<> onCancelSent;
  folly::Baton<> onCancelReceivedToserver;
  folly::Baton<> onCancelReceivedToclient;
  folly::Baton<> onRequestReceived;
  folly::Baton<> clientFinished;
  folly::Baton<> serverFinished;
};

using namespace yarpl::mocks;
using namespace ::testing;

#define LOCKSTEP_DEBUG(expr) VLOG(3) << expr

class LockstepAsyncHandler : public rsocket::RSocketResponder {
  LockstepBatons& batons_;
  Sequence& subscription_seq_;
  folly::ScopedEventBaseThread worker_;

 public:
  LockstepAsyncHandler(LockstepBatons& batons, Sequence& subscription_seq)
      : batons_(batons), subscription_seq_(subscription_seq){}

  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload p, StreamId)
      override {
    EXPECT_EQ(p.moveDataToString(), "initial");

    auto step1 = Flowable<Payload>::empty()->doOnComplete([this]() {
      CHECK_WAIT(this->batons_.onRequestReceived);
      LOCKSTEP_DEBUG("SERVER: sending onNext(foo)");
    });

    auto step2 = Flowable<>::justOnce(Payload("foo"))->doOnComplete([this]() {
      CHECK_WAIT(this->batons_.onCancelSent);
      CHECK_WAIT(this->batons_.onCancelReceivedToserver);
      LOCKSTEP_DEBUG("SERVER: sending onNext(bar)");
    });

    auto step3 = Flowable<>::justOnce(Payload("bar"))->doOnComplete([this]() {
      this->batons_.onSecondPayloadSent.post();
      LOCKSTEP_DEBUG("SERVER: sending onComplete()");
    });

    auto generator = Flowable<>::concat(step1, step2, step3)
                         ->doOnComplete([this]() {
                           LOCKSTEP_DEBUG("SERVER: posting serverFinished");
                           this->batons_.serverFinished.post();
                         })
                         ->subscribeOn(*worker_.getEventBase());

    // checked once the subscription is destroyed
    auto requestCheckpoint = std::make_shared<MockFunction<void(int64_t)>>();
    EXPECT_CALL(*requestCheckpoint, Call(2))
        .InSequence(this->subscription_seq_)
        .WillOnce(Invoke([=](auto n) {
          LOCKSTEP_DEBUG("SERVER: got request(" << n << ")");
          EXPECT_EQ(n, 2);
          this->batons_.onRequestReceived.post();
        }));

    auto cancelCheckpoint = std::make_shared<MockFunction<void()>>();
    EXPECT_CALL(*cancelCheckpoint, Call())
        .InSequence(this->subscription_seq_)
        .WillOnce(Invoke([=] {
          LOCKSTEP_DEBUG("SERVER: received cancel()");
          this->batons_.onCancelReceivedToclient.post();
          this->batons_.onCancelReceivedToserver.post();
        }));

    return generator
        ->doOnRequest(
            [requestCheckpoint](auto n) { requestCheckpoint->Call(n); })
        ->doOnCancel([cancelCheckpoint] { cancelCheckpoint->Call(); });
  }
};

// FIXME: This hits an ASAN heap-use-after-free.  Disabling for now, but we need
// to get back to this and fix it.
TEST(RequestStreamTest, DISABLED_OperationsAfterCancel) {
  LockstepBatons batons;
  Sequence server_seq;
  Sequence client_seq;

  auto server =
      makeServer(std::make_shared<LockstepAsyncHandler>(batons, server_seq));
  folly::ScopedEventBaseThread worker;
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto subscriber_mock =
      std::make_shared<testing::StrictMock<yarpl::mocks::MockSubscriber<std::string>>>(
          0);

  std::shared_ptr<Subscription> subscription;
  EXPECT_CALL(*subscriber_mock, onSubscribe_(_))
      .InSequence(client_seq)
      .WillOnce(Invoke([&](auto s) {
        LOCKSTEP_DEBUG("CLIENT: got onSubscribe(), sending request(2)");
        EXPECT_NE(s, nullptr);
        subscription = s;
        subscription->request(2);
      }));
  EXPECT_CALL(*subscriber_mock, onNext_("foo"))
      .InSequence(client_seq)
      .WillOnce(Invoke([&](auto) {
        EXPECT_NE(subscription, nullptr);
        LOCKSTEP_DEBUG("CLIENT: got onNext(foo), sending cancel()");
        subscription->cancel();
        batons.onCancelSent.post();
        CHECK_WAIT(batons.onCancelReceivedToclient);
        CHECK_WAIT(batons.onSecondPayloadSent);
        batons.clientFinished.post();
      }));

  // shouldn't receive 'bar', we canceled syncronously with the Subscriber
  // had 'cancel' been called in a different thread with no synchronization,
  // the client's Subscriber _could_ have received 'bar'

  LOCKSTEP_DEBUG("RUNNER: doing requestStream()");
  requester->requestStream(Payload("initial"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(subscriber_mock);

  CHECK_WAIT(batons.clientFinished);
  CHECK_WAIT(batons.serverFinished);
  LOCKSTEP_DEBUG("RUNNER: finished!");
}
