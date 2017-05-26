// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/internal/AllowanceSemaphore.h"

using namespace ::testing;
using namespace ::rsocket;

TEST(AllowanceSemaphoreTest, Finite) {
  AllowanceSemaphore sem;

  ASSERT_FALSE(sem.canAcquire());
  ASSERT_FALSE(sem.tryAcquire());

  ASSERT_EQ(0U, sem.release(1));
  ASSERT_FALSE(sem.canAcquire(2));
  ASSERT_TRUE(sem.canAcquire());
  ASSERT_TRUE(sem.tryAcquire());

  ASSERT_EQ(0U, sem.release(2));
  ASSERT_EQ(2U, sem.release(1));
  ASSERT_EQ(3U, sem.drain());
  ASSERT_EQ(0U, sem.drain());

  ASSERT_EQ(0U, sem.release(2));
  ASSERT_FALSE(sem.canAcquire(3));
  ASSERT_FALSE(sem.tryAcquire(3));
  ASSERT_TRUE(sem.canAcquire(2));
  ASSERT_TRUE(sem.tryAcquire(2));
  ASSERT_FALSE(sem.canAcquire());
}

TEST(AllowanceSemaphoreTest, DrainWithLimit) {
  AllowanceSemaphore sem;

  ASSERT_EQ(0U, sem.release(9));
  ASSERT_EQ(4U, sem.drainWithLimit(4));
  ASSERT_EQ(1U, sem.drainWithLimit(1));
  ASSERT_EQ(4U, sem.drainWithLimit(100));
}
