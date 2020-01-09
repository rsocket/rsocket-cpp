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
#include <iostream>
#include <thread>

#include "RSocketTests.h"
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

#include "yarpl/test_utils/Mocks.h"

using namespace yarpl::flowable;
using namespace rsocket;
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

using namespace ::testing;

constexpr std::chrono::milliseconds timeout{100};

class LockstepAsyncHandler : public rsocket::RSocketResponder {
  LockstepBatons& batons_;
  Sequence& subscription_seq_;
  folly::ScopedEventBaseThread worker_;

 public:
  LockstepAsyncHandler(LockstepBatons& batons, Sequence& subscription_seq)
      : batons_(batons), subscription_seq_(subscription_seq) {}

  std::shared_ptr<Flowable<Payload>> handleRequestStream(Payload p, StreamId)
      override {
    EXPECT_EQ(p.moveDataToString(), "initial");

    auto step1 = Flowable<Payload>::empty()->doOnComplete([this]() {
      this->batons_.onRequestReceived.timed_wait(timeout);
      VLOG(3) << "SERVER: sending onNext(foo)";
    });

    auto step2 = Flowable<>::justOnce(Payload("foo"))->doOnComplete([this]() {
      this->batons_.onCancelSent.timed_wait(timeout);
      this->batons_.onCancelReceivedToserver.timed_wait(timeout);
      VLOG(3) << "SERVER: sending onNext(bar)";
    });

    auto step3 = Flowable<>::justOnce(Payload("bar"))->doOnComplete([this]() {
      this->batons_.onSecondPayloadSent.post();
      VLOG(3) << "SERVER: sending onComplete()";
    });

    auto generator = Flowable<>::concat(step1, step2, step3)
                         ->doOnComplete([this]() {
                           VLOG(3) << "SERVER: posting serverFinished";
                           this->batons_.serverFinished.post();
                         })
                         ->subscribeOn(*worker_.getEventBase());

    // checked once the subscription is destroyed
    auto requestCheckpoint = std::make_shared<MockFunction<void(int64_t)>>();
    EXPECT_CALL(*requestCheckpoint, Call(2))
        .InSequence(this->subscription_seq_)
        .WillOnce(Invoke([=](auto n) {
          VLOG(3) << "SERVER: got request(" << n << ")";
          EXPECT_EQ(n, 2);
          this->batons_.onRequestReceived.post();
        }));

    auto cancelCheckpoint = std::make_shared<MockFunction<void()>>();
    EXPECT_CALL(*cancelCheckpoint, Call())
        .InSequence(this->subscription_seq_)
        .WillOnce(Invoke([=] {
          VLOG(3) << "SERVER: received cancel()";
          this->batons_.onCancelReceivedToclient.post();
          this->batons_.onCancelReceivedToserver.post();
        }));

    return generator
        ->doOnRequest(
            [requestCheckpoint](auto n) { requestCheckpoint->Call(n); })
        ->doOnCancel([cancelCheckpoint] { cancelCheckpoint->Call(); });
  }
};

TEST(RequestStreamTest, OperationsAfterCancel) {
  LockstepBatons batons;
  Sequence server_seq;
  Sequence client_seq;

  auto server =
      makeServer(std::make_shared<LockstepAsyncHandler>(batons, server_seq));
  folly::ScopedEventBaseThread worker;
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  auto subscriber_mock = std::make_shared<
      testing::StrictMock<yarpl::mocks::MockSubscriber<std::string>>>(0);

  std::shared_ptr<Subscription> subscription;
  EXPECT_CALL(*subscriber_mock, onSubscribe_(_))
      .InSequence(client_seq)
      .WillOnce(Invoke([&](auto s) {
        VLOG(3) << "CLIENT: got onSubscribe(), sending request(2)";
        EXPECT_NE(s, nullptr);
        subscription = s;
        subscription->request(2);
      }));
  EXPECT_CALL(*subscriber_mock, onNext_("foo"))
      .InSequence(client_seq)
      .WillOnce(Invoke([&](auto) {
        EXPECT_NE(subscription, nullptr);
        VLOG(3) << "CLIENT: got onNext(foo), sending cancel()";
        subscription->cancel();
        batons.onCancelSent.post();
        batons.onCancelReceivedToclient.timed_wait(timeout);
        batons.onSecondPayloadSent.timed_wait(timeout);
        batons.clientFinished.post();
      }));

  // shouldn't receive 'bar', we canceled syncronously with the Subscriber
  // had 'cancel' been called in a different thread with no synchronization,
  // the client's Subscriber _could_ have received 'bar'

  VLOG(3) << "RUNNER: doing requestStream()";
  requester->requestStream(Payload("initial"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(subscriber_mock);

  batons.clientFinished.timed_wait(timeout);
  batons.serverFinished.timed_wait(timeout);
  VLOG(3) << "RUNNER: finished!";
}
