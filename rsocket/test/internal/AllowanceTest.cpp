// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "rsocket/internal/Allowance.h"

using namespace ::testing;
using namespace ::rsocket;

TEST(AllowanceTest, Finite) {
  Allowance allowance;

  ASSERT_FALSE(allowance.canConsume(1));
  ASSERT_FALSE(allowance.tryConsume(1));

  ASSERT_EQ(0U, allowance.add(1));
  ASSERT_FALSE(allowance.canConsume(2));
  ASSERT_TRUE(allowance.canConsume(1));
  ASSERT_TRUE(allowance.tryConsume(1));

  ASSERT_EQ(0U, allowance.add(2));
  ASSERT_EQ(2U, allowance.add(1));
  ASSERT_EQ(3U, allowance.consumeAll());
  ASSERT_EQ(0U, allowance.consumeAll());

  ASSERT_EQ(0U, allowance.add(2));
  ASSERT_FALSE(allowance.canConsume(3));
  ASSERT_FALSE(allowance.tryConsume(3));
  ASSERT_TRUE(allowance.canConsume(2));
  ASSERT_TRUE(allowance.tryConsume(2));
  ASSERT_FALSE(allowance.canConsume(1));
}

TEST(AllowanceTest, ConsumeWithLimit) {
  Allowance allowance;

  ASSERT_EQ(0U, allowance.add(9));
  ASSERT_EQ(4U, allowance.consumeUpTo(4));
  ASSERT_EQ(1U, allowance.consumeUpTo(1));
  ASSERT_EQ(4U, allowance.consumeUpTo(100));
}
