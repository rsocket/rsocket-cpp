// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/SmartPointers.h"
#include "test/streams/Mocks.h"

using namespace ::testing;
using namespace ::reactivestreams;

TEST(SubscriberPtrTest, ResetDtorRelease) {
  auto subscriber0 = std::make_shared<StrictMock<MockSubscriber<int, int*>>>();
  auto subscriber1 = std::make_shared<StrictMock<MockSubscriber<int, int*>>>();
  auto subscriber2 = std::make_shared<StrictMock<MockSubscriber<int, int*>>>();
  {
    InSequence dummy;
    // It's not okay to skip ::onSubscribe signal, but we can afford to do it
    // with the mock.
    EXPECT_CALL(*subscriber0, onComplete_());
    EXPECT_CALL(*subscriber1, onComplete_());
    EXPECT_CALL(*subscriber2, onComplete_()).Times(0);
  }

  {
    auto ptr = makeSubscriberPtr(subscriber0);
    ptr.reset();
    ASSERT_TRUE(!ptr);
    ptr.reset(subscriber1);
    // Wait for d'tor to kick in.
  }
  auto ptr = makeSubscriberPtr(subscriber2);
  ptr.release();
  ptr.onComplete();
}

TEST(SubscriberPtrTest, IdempotentComplete) {
  auto subscriber = std::make_shared<StrictMock<MockSubscriber<int, int*>>>();
  {
    InSequence dummy;
    // It's not okay to skip ::onSubscribe signal
    EXPECT_CALL(*subscriber, onNext_(_)).Times(2);
    EXPECT_CALL(*subscriber, onComplete_());
    EXPECT_CALL(*subscriber, onError_(_)).Times(0);
  }

  auto ptr = makeSubscriberPtr(subscriber);
  ASSERT_TRUE(!!ptr);
  ASSERT_EQ(subscriber, static_cast<decltype(ptr)::SharedPtrT>(ptr));

  int value = 42;
  ptr.onNext(value);
  ptr.onNext(value);
  ptr.onComplete();
  ptr.onError(nullptr);
  ptr.onComplete();
}

TEST(SubscriberPtrTest, IdempotentError) {
  auto subscriber = std::make_shared<StrictMock<MockSubscriber<int, int*>>>();
  {
    InSequence dummy;
    // It's not okay to skip ::onSubscribe signal
    EXPECT_CALL(*subscriber, onNext_(_)).Times(2);
    EXPECT_CALL(*subscriber, onError_(_));
    EXPECT_CALL(*subscriber, onComplete_()).Times(0);
  }

  auto ptr = makeSubscriberPtr(subscriber);
  ASSERT_TRUE(!!ptr);
  ASSERT_EQ(subscriber, static_cast<decltype(ptr)::SharedPtrT>(ptr));

  int value = 42;
  ptr.onNext(value);
  ptr.onNext(value);
  ptr.onError(nullptr);
  ptr.onComplete();
  ptr.onError(nullptr);
}

TEST(SubscriptionPtrTest, IdempotentCancel) {
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  {
    InSequence dummy;
    EXPECT_CALL(*subscription, request_(1));
    EXPECT_CALL(*subscription, request_(2));
    EXPECT_CALL(*subscription, cancel_());
  }

  auto ptr = makeSubscriptionPtr(subscription);
  ASSERT_TRUE(!!ptr);
  ASSERT_EQ(subscription, static_cast<decltype(ptr)::SharedPtrT>(ptr));

  ptr.request(1);
  ptr.request(2);
  ptr.cancel();
  ptr.cancel();
}

TEST(SubscriptionPtrTest, ResetDtorRelease) {
  auto subscription0 = std::make_shared<StrictMock<MockSubscription>>();
  auto subscription1 = std::make_shared<StrictMock<MockSubscription>>();
  auto subscription2 = std::make_shared<StrictMock<MockSubscription>>();
  {
    InSequence dummy;
    EXPECT_CALL(*subscription0, cancel_());
    EXPECT_CALL(*subscription1, cancel_());
    EXPECT_CALL(*subscription2, cancel_()).Times(0);
  }

  {
    auto ptr = makeSubscriptionPtr(subscription0);
    ptr.reset();
    ASSERT_TRUE(!ptr);
    ptr.reset(subscription1);
    // Wait for d'tor to kick in.
  }
  auto ptr = makeSubscriptionPtr(subscription2);
  ptr.release();
  ptr.cancel();
}
