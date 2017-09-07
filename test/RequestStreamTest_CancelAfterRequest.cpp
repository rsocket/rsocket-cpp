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
struct Batons {
  folly::Baton<> serverFinished;
  folly::Baton<> clientFinished;

  void reset() {
    serverFinished.reset();
    clientFinished.reset();
  }
};

class RaceyRequestAfterCancelTestHandler : public rsocket::RSocketResponder {
 private:
  class RaceySubscription : public ::yarpl::flowable::Subscription {
   public:
    RaceySubscription(Reference<Subscriber<Payload>> s, Batons& batons)
        : subscriber_(std::move(s)), batons_(batons) {}

    void request(int64_t n) {
      LOCKSTEP_DEBUG("SERVER: recieve request(" << n << ")");
      for (int i = 0; i < n; i++) {
        subscriber_->onNext(Payload("f"));
      }
    }

    void cancel() {
      LOCKSTEP_DEBUG("SERVER: recieve cancel(), sending onComplete()");
      subscriber_->onComplete();
      subscriber_ = nullptr;
      this->batons_.serverFinished.post();
    }

   private:
    Reference<Subscriber<Payload>> subscriber_;
    Batons& batons_;
  };

 public:
  RaceyRequestAfterCancelTestHandler(Batons& batons) : batons_(batons) {}

  Reference<Flowable<Payload>> handleRequestStream(Payload p, StreamId)
      override {
    EXPECT_EQ(p.moveDataToString(), "initial");
    return Flowables::fromPublisher<Payload>(
        [this](Reference<flowable::Subscriber<Payload>> subscriber) {

          LOCKSTEP_DEBUG("SERVER: sending onSubscribe()");
          auto subscription = make_ref<RaceySubscription>(subscriber, batons_);
          subscriber->onSubscribe(subscription);
        });
  }

 private:
  Batons& batons_;
};

// number to request each request(n) call
auto const will_request = 100;

TEST(RequestStreamTest, RaceyRequestAfterCancel) {
  std::mt19937 rand(123); // we want a PRNG here
  std::uniform_int_distribution<> dis_0_100(0, 100);

  Batons batons;
  folly::ScopedEventBaseThread worker;
  auto server =
      makeServer(std::make_shared<RaceyRequestAfterCancelTestHandler>(batons));
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();

  for (int i = 0; i < 1000; i++) {
    auto at_response = 0;
    Reference<Subscription> subscription;

    auto subscriber = Subscribers::create<std::string>(
        // onSubscribe
        [&](Reference<Subscription> subscription_) {
          subscription = subscription_;
          subscription->request(will_request);

          auto chance = dis_0_100(rand);
          LOCKSTEP_DEBUG(
              "CLIENT: got onSubscribe(), sending request(" << will_request
                                                            << "), rand: "
                                                            << chance);
          // 1% chance subscription is canceled immediately in the same thread
          if (chance == 0) {
            LOCKSTEP_DEBUG("CLIENT: immediately sending cancel()");
            subscription->cancel();
          }
          // 99% chance subscription is canceled from another thread (with a
          // random delay)
          else {
            std::thread([&] {
              std::this_thread::sleep_for(
                  std::chrono::nanoseconds(dis_0_100(rand)));
              LOCKSTEP_DEBUG("CLIENT: delayed sending cancel()");
              subscription->cancel();
            }).detach();
          }
        },

        // onNext
        [&](auto) {
          if (++at_response == will_request) {
            LOCKSTEP_DEBUG(
                "CLIENT: sending request(" << will_request << ") again");
            at_response = 0;
            subscription->request(
                will_request); // we *might* be canceled here just be a no-op
          }
        },

        // onError
        [&](auto) { FAIL(); },

        // onComplete
        [&]() {
          LOCKSTEP_DEBUG(
              "CLIENT: got onComplete(), ended with "
              << at_response
              << " recieved in current batch");
          batons.clientFinished.post();
        });

    LOCKSTEP_DEBUG("RUNNER: doing requestStream()");
    requester->requestStream(Payload("initial"))
        ->map([](auto p) { return p.moveDataToString(); })
        ->subscribe(subscriber);

    CHECK_WAIT_FOR(batons.clientFinished, 1000);
    CHECK_WAIT_FOR(batons.serverFinished, 1000);
    LOCKSTEP_DEBUG("RUNNER: finished!");
    batons.reset();
  }
}
}
