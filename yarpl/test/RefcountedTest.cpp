// Copyright 2004-present Facebook. All Rights Reserved.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "yarpl/Refcounted.h"

namespace yarpl {

TEST(RefcountedTest, ReferenceCountingWorks) {
  auto first = make_ref<Refcounted>();
  EXPECT_EQ(1U, first.use_count());

  auto second = first;

  EXPECT_EQ(second.get(), first.get());
  EXPECT_EQ(2U, first.use_count());

  auto third = std::move(second);
  EXPECT_EQ(nullptr, second.get());
  EXPECT_EQ(third.get(), first.get());
  EXPECT_EQ(2U, first.use_count());

  // second was already moved from, above.
  second.reset();
  EXPECT_EQ(nullptr, second.get());
  EXPECT_EQ(2U, first.use_count());

  auto fourth = third;
  EXPECT_EQ(3U, first.use_count());

  fourth.reset();
  EXPECT_EQ(nullptr, fourth.get());
  EXPECT_EQ(2U, first.use_count());
}

} // yarpl
