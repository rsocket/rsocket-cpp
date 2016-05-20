// Copyright 2004-present Facebook. All Rights Reserved.

#include "reactive-streams-cpp/Mocks.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;
using namespace reactivestreams;

TEST(Examples, SelfManagedMocks) {
  // Best run with ASAN, to detect potential leaks, use-after-free or
  // double-free bugs.
  int value = 42;

  UnmanagedMockPublisher<int> producer;
  auto* subscription = &makeMockSubscription();
  auto* subscriber = &makeMockSubscriber<int>();
  {
    InSequence dummy;
    EXPECT_CALL(producer, subscribe_(subscriber))
        .WillOnce(Invoke([&](Subscriber<int>* consumer) {
          consumer->onSubscribe(*subscription);
        }));
    EXPECT_CALL(*subscriber, onSubscribe_(subscription));
    EXPECT_CALL(*subscription, request_(1));
    EXPECT_CALL(*subscription, cancel_())
        .WillOnce(Invoke([&]() {
          // We must have received Subscription::request(1), hence we can
          // deliver one element, despite Subscription::cancel() has been
          // called.
          subscriber->onNext(value);
          // This Publisher never spontaneously terminates the subscription,
          // hence we can respond with onComplete unconditionally.
          subscriber->onComplete();
          subscriber = nullptr;
        }));
    EXPECT_CALL(*subscriber, onNext_(value));
    EXPECT_CALL(*subscriber, onComplete_());
  }
  producer.subscribe(*subscriber);
  subscription->request(1);
  subscription->cancel();
  subscription = nullptr;
}

TEST(Examples, UnmanagedMocks) {
  // Best run with ASAN, to detect potential leaks, use-after-free or
  // double-free bugs.
  int value = 42;

  UnmanagedMockPublisher<int> producer;
  UnmanagedMockSubscription subscription;
  UnmanagedMockSubscriber<int> subscriber;
  {
    InSequence dummy;
    EXPECT_CALL(producer, subscribe_(&subscriber))
        .WillOnce(Invoke([&](Subscriber<int>* consumer) {
          consumer->onSubscribe(subscription);
        }));
    EXPECT_CALL(subscriber, onSubscribe_(&subscription));
    EXPECT_CALL(subscription, request_(1));
    EXPECT_CALL(subscription, cancel_())
        .WillOnce(Invoke([&]() {
          // We must have received Subscription::request(1), hence we can
          // deliver one element, despite Subscription::cancel() has been
          // called.
          subscriber.onNext(value);
          // This Publisher never spontaneously terminates the subscription,
          // hence we can respond with onComplete unconditionally.
          subscriber.onComplete();
        }));
    EXPECT_CALL(subscriber, onNext_(value));
    EXPECT_CALL(subscriber, onComplete_());
  }
  producer.subscribe(subscriber);
  subscription.request(1);
  subscription.cancel();
}
