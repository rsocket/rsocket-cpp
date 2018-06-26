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

#include <gtest/gtest.h>
#include <thread>

#include "yarpl/utils/credits.h"

using namespace yarpl::credits;

TEST(Credits, addSmall) {
  std::atomic<std::int64_t> rn{0};
  add(&rn, 10);
  ASSERT_EQ(rn, 10);
}

TEST(Credits, addSmall2) {
  std::atomic<std::int64_t> rn{0};
  add(&rn, 10);
  add(&rn, 5);
  add(&rn, 20);
  ASSERT_EQ(rn, 35);
}

TEST(Credits, addOverflow) {
  std::atomic<std::int64_t> rn{0};
  add(&rn, INT64_MAX);
  ASSERT_EQ(rn, INT64_MAX);
}

TEST(Credits, addOverflow2) {
  std::atomic<std::int64_t> rn{6789};
  add(&rn, INT64_MAX);
  ASSERT_EQ(rn, INT64_MAX);
}

TEST(Credits, addOverflow3) {
  std::atomic<std::int64_t> rn{INT64_MAX};
  add(&rn, INT64_MAX);
  ASSERT_EQ(rn, INT64_MAX);
}

TEST(Credits, addNegative) {
  std::atomic<std::int64_t> rn{0};
  add(&rn, -9876);
  ASSERT_EQ(rn, 0);
}

TEST(Credits, addNegative2) {
  std::atomic<std::int64_t> rn{9999};
  add(&rn, -9876);
  ASSERT_EQ(rn, 9999);
}

TEST(Credits, cancel) {
  std::atomic<std::int64_t> rn{9999};
  bool didCancel = cancel(&rn);
  ASSERT_EQ(rn, INT64_MIN);
  ASSERT_TRUE(isCancelled(&rn));
  ASSERT_TRUE(didCancel);
}

TEST(Credits, cancel2) {
  std::atomic<std::int64_t> rn{9999};
  cancel(&rn);
  bool didCancel = cancel(&rn);
  ASSERT_EQ(rn, INT64_MIN);
  ASSERT_TRUE(isCancelled(&rn));
  ASSERT_FALSE(didCancel);
}

TEST(Credits, cancel3) {
  std::atomic<std::int64_t> rn{9999};
  cancel(&rn);
  // it should stay cancelled once cancelled
  add(&rn, 1);
  ASSERT_TRUE(isCancelled(&rn));
}

TEST(Credits, isInfinite) {
  std::atomic<std::int64_t> rn{0};
  add(&rn, INT64_MAX);
  ASSERT_TRUE(isInfinite(&rn));
}

TEST(Credits, consumeSmall) {
  std::atomic<std::int64_t> rn{100};
  consume(&rn, 10);
  ASSERT_EQ(rn, 90);
}

TEST(Credits, consumeExact) {
  std::atomic<std::int64_t> rn{100};
  consume(&rn, 100);
  ASSERT_EQ(rn, 0);
}

TEST(Credits, consumeTooMany) {
  std::atomic<std::int64_t> rn{100};
  consume(&rn, 110);
  ASSERT_EQ(rn, 0);
}
