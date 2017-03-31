// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include "yarpl/flowable/utils/SubscriptionHelper.h"

using namespace yarpl::flowable::internal;

TEST(SubscriptionHelper, addSmall) {
  std::atomic<std::int64_t> rn{0};
  SubscriptionHelper::addCredits(&rn, 10);
  ASSERT_EQ(rn, 10);
}

TEST(SubscriptionHelper, addSmall2) {
  std::atomic<std::int64_t> rn{0};
  SubscriptionHelper::addCredits(&rn, 10);
  SubscriptionHelper::addCredits(&rn, 5);
  SubscriptionHelper::addCredits(&rn, 20);
  ASSERT_EQ(rn, 35);
}

TEST(SubscriptionHelper, addOverflow) {
  std::atomic<std::int64_t> rn{0};
  SubscriptionHelper::addCredits(&rn, INT64_MAX);
  ASSERT_EQ(rn, INT64_MAX);
}

TEST(SubscriptionHelper, addOverflow2) {
  std::atomic<std::int64_t> rn{6789};
  SubscriptionHelper::addCredits(&rn, INT64_MAX);
  ASSERT_EQ(rn, INT64_MAX);
}

TEST(SubscriptionHelper, addOverflow3) {
  std::atomic<std::int64_t> rn{INT64_MAX};
  SubscriptionHelper::addCredits(&rn, INT64_MAX);
  ASSERT_EQ(rn, INT64_MAX);
}

TEST(SubscriptionHelper, addNegative) {
  std::atomic<std::int64_t> rn{0};
  SubscriptionHelper::addCredits(&rn, -9876);
  ASSERT_EQ(rn, 0);
}

TEST(SubscriptionHelper, addNegative2) {
  std::atomic<std::int64_t> rn{9999};
  SubscriptionHelper::addCredits(&rn, -9876);
  ASSERT_EQ(rn, 9999);
}

TEST(SubscriptionHelper, addCancel) {
  std::atomic<std::int64_t> rn{9999};
  bool didCancel = SubscriptionHelper::addCancel(&rn);
  ASSERT_EQ(rn, INT64_MIN);
  ASSERT_TRUE(SubscriptionHelper::isCancelled(&rn));
  ASSERT_TRUE(didCancel);
}

TEST(SubscriptionHelper, addCancel2) {
  std::atomic<std::int64_t> rn{9999};
  SubscriptionHelper::addCancel(&rn);
  bool didCancel = SubscriptionHelper::addCancel(&rn);
  ASSERT_EQ(rn, INT64_MIN);
  ASSERT_TRUE(SubscriptionHelper::isCancelled(&rn));
  ASSERT_FALSE(didCancel);
}

TEST(SubscriptionHelper, addCancel3) {
  std::atomic<std::int64_t> rn{9999};
  SubscriptionHelper::addCancel(&rn);
  // it should stay cancelled once cancelled
  SubscriptionHelper::addCredits(&rn, 1);
  ASSERT_TRUE(SubscriptionHelper::isCancelled(&rn));
}

TEST(SubscriptionHelper, isInfinite) {
  std::atomic<std::int64_t> rn{0};
  SubscriptionHelper::addCredits(&rn, INT64_MAX);
  ASSERT_TRUE(SubscriptionHelper::isInfinite(&rn));
}

TEST(SubscriptionHelper, consumeSmall) {
  std::atomic<std::int64_t> rn{100};
  SubscriptionHelper::consumeCredits(&rn, 10);
  ASSERT_EQ(rn, 90);
}

TEST(SubscriptionHelper, consumeExact) {
  std::atomic<std::int64_t> rn{100};
  SubscriptionHelper::consumeCredits(&rn, 100);
  ASSERT_EQ(rn, 0);
}

TEST(SubscriptionHelper, consumeTooMany) {
  std::atomic<std::int64_t> rn{100};
  SubscriptionHelper::consumeCredits(&rn, 110);
  ASSERT_EQ(rn, 0);
}
