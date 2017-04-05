// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <gtest/gtest.h>
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/versions/FrameSerializer_v0_1.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(PayloadTest, EmptyMetadata) {
  Payload p("some error message");
  EXPECT_NE(p.data, nullptr);
  EXPECT_EQ(p.metadata, nullptr);
}

TEST(PayloadTest, Clear) {
  Payload p("hello");
  ASSERT_TRUE(p);

  p.clear();
  ASSERT_FALSE(p);
}

TEST(PayloadTest, GiantMetadata) {
  constexpr auto metadataSize = std::numeric_limits<uint32_t>::max();

  auto metadata = folly::IOBuf::wrapBuffer(&metadataSize, sizeof(metadataSize));
  folly::io::Cursor cur(metadata.get());

  EXPECT_THROW(
      FrameSerializerV0_1::deserializeMetadataFrom(cur, FrameFlags::METADATA),
      std::runtime_error);
}
