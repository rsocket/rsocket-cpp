// Copyright 2004-present Facebook. All Rights Reserved.

#include <glog/logging.h>
#include <gtest/gtest.h>
#include "rsocket/internal/Common.h"

using namespace testing;
using namespace rsocket;

TEST(ResumeIdentificationTokenTest, Conversion) {
  for (int i = 0; i < 10; i++) {
    auto token = ResumeIdentificationToken::generateNew();
    auto token2 = ResumeIdentificationToken(token.str());
    CHECK_EQ(token, token2);
    CHECK_EQ(token.str(), token2.str());
  }
}
