// Copyright 2004-present Facebook. All Rights Reserved.

#include <utility>

#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>

#include "src/Frame.h"

using namespace ::testing;
using namespace ::reactivesocket;

// TODO(stupaq): tests with malformed frames

template <typename Frame, typename... Args>
Frame reserialize(Args... args) {
  Frame frame;
  EXPECT_TRUE(
      frame.deserializeFrom(Frame(std::forward<Args>(args)...).serializeOut()));
  return frame;
}

template <typename Frame>
void expectHeader(
    FrameType type,
    FrameFlags flags,
    StreamId streamId,
    const Frame& frame) {
  EXPECT_EQ(type, frame.header_.type_);
  EXPECT_EQ(streamId, frame.header_.streamId_);
  EXPECT_EQ(flags, frame.header_.flags_);
}

TEST(FrameTest, Frame_REQUEST_STREAM) {
  uint32_t streamId = 42;
  FrameFlags flags =
      FrameFlags_COMPLETE | FrameFlags_REQN_PRESENT | FrameFlags_METADATA;
  uint32_t requestN = 3;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_STREAM>(
      streamId, flags, requestN, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_STREAM, flags, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_REQUEST_CHANNEL) {
  uint32_t streamId = 42;
  FrameFlags flags =
      FrameFlags_COMPLETE | FrameFlags_REQN_PRESENT | FrameFlags_METADATA;
  uint32_t requestN = 3;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_CHANNEL>(
      streamId, flags, requestN, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_CHANNEL, flags, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_REQUEST_N) {
  uint32_t streamId = 42;
  uint32_t requestN = 24;
  auto frame = reserialize<Frame_REQUEST_N>(streamId, requestN);

  expectHeader(FrameType::REQUEST_N, FrameFlags_EMPTY, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
}

TEST(FrameTest, Frame_CANCEL) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto frame = reserialize<Frame_CANCEL>(streamId, metadata->clone());

  expectHeader(FrameType::CANCEL, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.metadata_));
}

TEST(FrameTest, Frame_RESPONSE) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_COMPLETE | FrameFlags_METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_RESPONSE>(
      streamId, flags, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::RESPONSE, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_RESPONSE_NoMeta) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_COMPLETE;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame =
      reserialize<Frame_RESPONSE>(streamId, flags, Payload(data->clone()));

  expectHeader(FrameType::RESPONSE, flags, streamId, frame);
  EXPECT_FALSE(frame.payload_.metadata);
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_ERROR) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_METADATA;
  auto errorCode = ErrorCode::REJECTED;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("DAHTA");
  auto frame = reserialize<Frame_ERROR>(
      streamId, errorCode, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::ERROR, flags, streamId, frame);
  EXPECT_EQ(errorCode, frame.errorCode_);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_KEEPALIVE) {
  uint32_t streamId = 0;
  ResumePosition position = 101;
  auto flags = FrameFlags_KEEPALIVE_RESPOND;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_KEEPALIVE>(flags, position, data->clone());

  expectHeader(
      FrameType::KEEPALIVE, FrameFlags_KEEPALIVE_RESPOND, streamId, frame);
  EXPECT_EQ(position, frame.position_);
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.data_));
}

TEST(FrameTest, Frame_SETUP) {
  FrameFlags flags = FrameFlags_EMPTY;
  uint32_t version = 0;
  uint32_t keepaliveTime = std::numeric_limits<uint32_t>::max();
  uint32_t maxLifetime = std::numeric_limits<uint32_t>::max();
  ResumeIdentificationToken::Data tokenData;
  tokenData.fill(1);
  ResumeIdentificationToken token;
  token.set(std::move(tokenData));
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_SETUP>(
      flags,
      version,
      keepaliveTime,
      maxLifetime,
      token,
      "md",
      "d",
      Payload(data->clone()));

  expectHeader(FrameType::SETUP, flags, 0, frame);
  EXPECT_EQ(version, frame.version_);
  EXPECT_EQ(keepaliveTime, frame.keepaliveTime_);
  EXPECT_EQ(maxLifetime, frame.maxLifetime_);
  // Token should be default constructed
  EXPECT_EQ(ResumeIdentificationToken(), frame.token_);
  EXPECT_EQ("md", frame.metadataMimeType_);
  EXPECT_EQ("d", frame.dataMimeType_);
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_SETUP_resume) {
  FrameFlags flags = FrameFlags_EMPTY | FrameFlags_RESUME_ENABLE;
  uint32_t version = 0;
  uint32_t keepaliveTime = std::numeric_limits<uint32_t>::max();
  uint32_t maxLifetime = std::numeric_limits<uint32_t>::max();
  ResumeIdentificationToken::Data tokenData;
  tokenData.fill(1);
  ResumeIdentificationToken token;
  token.set(std::move(tokenData));
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_SETUP>(
      flags,
      version,
      keepaliveTime,
      maxLifetime,
      token,
      "md",
      "d",
      Payload(data->clone()));

  expectHeader(FrameType::SETUP, flags, 0, frame);
  EXPECT_EQ(version, frame.version_);
  EXPECT_EQ(keepaliveTime, frame.keepaliveTime_);
  EXPECT_EQ(maxLifetime, frame.maxLifetime_);
  EXPECT_EQ(token, frame.token_);
  EXPECT_EQ("md", frame.metadataMimeType_);
  EXPECT_EQ("d", frame.dataMimeType_);
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_LEASE) {
  FrameFlags flags = FrameFlags_EMPTY;
  uint32_t ttl = std::numeric_limits<uint32_t>::max();
  int32_t numberOfRequests = Frame_REQUEST_N::kMaxRequestN;
  auto frame = reserialize<Frame_LEASE>(ttl, numberOfRequests);

  expectHeader(FrameType::LEASE, flags, 0, frame);
  EXPECT_EQ(ttl, frame.ttl_);
  EXPECT_EQ(numberOfRequests, frame.numberOfRequests_);
}

TEST(FrameTest, Frame_REQUEST_RESPONSE) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_RESPONSE>(
      streamId, flags, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_RESPONSE, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_REQUEST_FNF) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_FNF>(
      streamId, flags, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_FNF, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_METADATA_PUSH) {
  FrameFlags flags = FrameFlags_METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto frame = reserialize<Frame_METADATA_PUSH>(metadata->clone());

  expectHeader(FrameType::METADATA_PUSH, flags, 0, frame);
  EXPECT_TRUE(folly::IOBufEqual()(*metadata, *frame.metadata_));
}
