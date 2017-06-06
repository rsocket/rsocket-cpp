// Copyright 2004-present Facebook. All Rights Reserved.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "yarpl/Refcounted.h"

namespace yarpl {

TEST(RefcountedTest, ObjectCountsAreMaintained) {
  std::vector<std::unique_ptr<Refcounted>> v;
  for (std::size_t i = 0; i < 16; ++i) {
    v.push_back(std::make_unique<Refcounted>());
    EXPECT_EQ(0U, v[i]->count()); // no references.
  }

  v.resize(11);
}

TEST(RefcountedTest, ReferenceCountingWorks) {
  auto first = Reference<Refcounted>(new Refcounted);
  EXPECT_EQ(1U, first->count());

  auto second = first;

  EXPECT_EQ(second.get(), first.get());
  EXPECT_EQ(2U, first->count());

  auto third = std::move(second);
  EXPECT_EQ(nullptr, second.get());
  EXPECT_EQ(third.get(), first.get());
  EXPECT_EQ(2U, first->count());

  // second was already moved from, above.
  second.reset();
  EXPECT_EQ(nullptr, second.get());
  EXPECT_EQ(2U, first->count());

  auto fourth = third;
  EXPECT_EQ(3U, first->count());

  fourth.reset();
  EXPECT_EQ(nullptr, fourth.get());
  EXPECT_EQ(2U, first->count());
}
} // yarpl
