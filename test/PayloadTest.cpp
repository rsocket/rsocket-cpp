// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "src/Payload.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(PayloadTest, Clear) {
  Payload p("hello");
  ASSERT_TRUE(p);

  p.clear();
  ASSERT_FALSE(p);
}
