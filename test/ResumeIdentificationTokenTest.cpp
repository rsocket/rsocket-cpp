// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/Common.h"
#include "test/streams/Mocks.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(ResumeIdentificationTokenTest, ToString) {
  ResumeIdentificationToken token;
  token.set({{0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xff, 0xed, 0xcb, 0xa9, 0x87, 0x65, 0x43, 0x21, 0x00}});
  ASSERT_EQ("123456789abcdeffedcba98765432100", token.toString());
}
