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

#include "rsocket/internal/Allowance.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

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
