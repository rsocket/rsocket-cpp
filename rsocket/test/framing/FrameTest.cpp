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

#include <utility>

#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>

#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameSerializer.h"

using namespace ::rsocket;

template <typename Frame, typename... Args>
Frame reserialize(Args... args) {
  Frame givenFrame = Frame(std::forward<Args>(args)...);
  auto frameSerializer =
      FrameSerializer::createFrameSerializer(ProtocolVersion::Latest);
  auto serializedFrame = frameSerializer->serializeOut(std::move(givenFrame));
  Frame newFrame;
  EXPECT_TRUE(
      frameSerializer->deserializeFrom(newFrame, std::move(serializedFrame)));
  return newFrame;
}

template <typename Frame>
void expectHeader(
    FrameType type,
    FrameFlags flags,
    StreamId streamId,
    const Frame& frame) {
  EXPECT_EQ(type, frame.header_.type);
  EXPECT_EQ(streamId, frame.header_.streamId);
  EXPECT_EQ(flags, frame.header_.flags);
}

TEST(FrameTest, Frame_REQUEST_STREAM) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::COMPLETE | FrameFlags::METADATA;
  uint32_t requestN = 3;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_STREAM>(
      streamId, flags, requestN, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_STREAM, flags, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_REQUEST_CHANNEL) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::COMPLETE | FrameFlags::METADATA;
  uint32_t requestN = 3;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_CHANNEL>(
      streamId, flags, requestN, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_CHANNEL, flags, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_REQUEST_N) {
  uint32_t streamId = 42;
  uint32_t requestN = 24;
  auto frame = reserialize<Frame_REQUEST_N>(streamId, requestN);

  expectHeader(FrameType::REQUEST_N, FrameFlags::EMPTY_, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
}

TEST(FrameTest, Frame_CANCEL) {
  uint32_t streamId = 42;
  auto frame = reserialize<Frame_CANCEL>(streamId);
  expectHeader(FrameType::CANCEL, FrameFlags::EMPTY_, streamId, frame);
}

TEST(FrameTest, Frame_PAYLOAD) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::COMPLETE | FrameFlags::METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_PAYLOAD>(
      streamId, flags, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::PAYLOAD, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_PAYLOAD_NoMeta) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::COMPLETE;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame =
      reserialize<Frame_PAYLOAD>(streamId, flags, Payload(data->clone()));

  expectHeader(FrameType::PAYLOAD, flags, streamId, frame);
  EXPECT_FALSE(frame.payload_.metadata);
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_ERROR) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::METADATA;
  auto errorCode = ErrorCode::REJECTED;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("DAHTA");
  auto frame = reserialize<Frame_ERROR>(
      streamId, errorCode, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::ERROR, flags, streamId, frame);
  EXPECT_EQ(errorCode, frame.errorCode_);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_KEEPALIVE) {
  uint32_t streamId = 0;
  ResumePosition position = 101;
  auto flags = FrameFlags::KEEPALIVE_RESPOND;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_KEEPALIVE>(flags, position, data->clone());

  expectHeader(
      FrameType::KEEPALIVE, FrameFlags::KEEPALIVE_RESPOND, streamId, frame);
  // Default position
  auto currProtVersion = ProtocolVersion::Latest;
  if (currProtVersion == ProtocolVersion(0, 1)) {
    EXPECT_EQ(0, frame.position_);
  } else if (currProtVersion == ProtocolVersion(1, 0)) {
    EXPECT_EQ(position, frame.position_);
  }
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.data_));
}

TEST(FrameTest, Frame_SETUP) {
  FrameFlags flags = FrameFlags::EMPTY_;
  uint16_t versionMajor = 4;
  uint16_t versionMinor = 5;
  uint32_t keepaliveTime = Frame_SETUP::kMaxKeepaliveTime;
  uint32_t maxLifetime = Frame_SETUP::kMaxLifetime;
  ResumeIdentificationToken token = ResumeIdentificationToken::generateNew();
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_SETUP>(
      flags,
      versionMajor,
      versionMinor,
      keepaliveTime,
      maxLifetime,
      token,
      "md",
      "d",
      Payload(data->clone()));

  expectHeader(FrameType::SETUP, flags, 0, frame);
  EXPECT_EQ(versionMajor, frame.versionMajor_);
  EXPECT_EQ(versionMinor, frame.versionMinor_);
  EXPECT_EQ(keepaliveTime, frame.keepaliveTime_);
  EXPECT_EQ(maxLifetime, frame.maxLifetime_);
  // Token should be default constructed
  EXPECT_EQ(ResumeIdentificationToken(), frame.token_);
  EXPECT_EQ("md", frame.metadataMimeType_);
  EXPECT_EQ("d", frame.dataMimeType_);
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_SETUP_resume) {
  FrameFlags flags = FrameFlags::EMPTY_ | FrameFlags::RESUME_ENABLE;
  uint16_t versionMajor = 0;
  uint16_t versionMinor = 0;
  uint32_t keepaliveTime = Frame_SETUP::kMaxKeepaliveTime;
  uint32_t maxLifetime = Frame_SETUP::kMaxLifetime;
  ResumeIdentificationToken token = ResumeIdentificationToken::generateNew();
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_SETUP>(
      flags,
      versionMajor,
      versionMinor,
      keepaliveTime,
      maxLifetime,
      token,
      "md",
      "d",
      Payload(data->clone()));

  expectHeader(FrameType::SETUP, flags, 0, frame);
  EXPECT_EQ(versionMajor, frame.versionMajor_);
  EXPECT_EQ(versionMinor, frame.versionMinor_);
  EXPECT_EQ(keepaliveTime, frame.keepaliveTime_);
  EXPECT_EQ(maxLifetime, frame.maxLifetime_);
  EXPECT_EQ(token, frame.token_);
  EXPECT_EQ("md", frame.metadataMimeType_);
  EXPECT_EQ("d", frame.dataMimeType_);
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_LEASE) {
  FrameFlags flags = FrameFlags::EMPTY_;
  uint32_t ttl = Frame_LEASE::kMaxTtl;
  auto numberOfRequests = Frame_LEASE::kMaxNumRequests;
  auto frame = reserialize<Frame_LEASE>(ttl, numberOfRequests);

  expectHeader(FrameType::LEASE, flags, 0, frame);
  EXPECT_EQ(ttl, frame.ttl_);
  EXPECT_EQ(numberOfRequests, frame.numberOfRequests_);
}

TEST(FrameTest, Frame_REQUEST_RESPONSE) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_RESPONSE>(
      streamId, flags, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_RESPONSE, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_REQUEST_FNF) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_FNF>(
      streamId, flags, Payload(data->clone(), metadata->clone()));

  expectHeader(FrameType::REQUEST_FNF, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.payload_.metadata));
  EXPECT_TRUE(folly::IOBufEqualTo()(*data, *frame.payload_.data));
}

TEST(FrameTest, Frame_METADATA_PUSH) {
  FrameFlags flags = FrameFlags::METADATA;
  auto metadata = folly::IOBuf::copyBuffer("i'm so meta even this acronym");
  auto frame = reserialize<Frame_METADATA_PUSH>(metadata->clone());

  expectHeader(FrameType::METADATA_PUSH, flags, 0, frame);
  EXPECT_TRUE(folly::IOBufEqualTo()(*metadata, *frame.metadata_));
}

TEST(FrameTest, Frame_RESUME) {
  FrameFlags flags = FrameFlags::EMPTY_;
  uint16_t versionMajor = 4;
  uint16_t versionMinor = 5;
  ResumeIdentificationToken token = ResumeIdentificationToken::generateNew();
  ResumePosition serverPosition = 6;
  ResumePosition clientPosition = 7;
  auto frame = reserialize<Frame_RESUME>(
      token,
      serverPosition,
      clientPosition,
      ProtocolVersion(versionMajor, versionMinor));

  expectHeader(FrameType::RESUME, flags, 0, frame);
  // TODO: enable when v1.0 is enabled by default
  // EXPECT_EQ(versionMajor, frame.versionMajor_);
  // EXPECT_EQ(versionMinor, frame.versionMinor_);
  // Token should be default constructed
  EXPECT_EQ(token, frame.token_);
  EXPECT_EQ(serverPosition, frame.lastReceivedServerPosition_);
  // TODO: enable when v1.0 is enabled by default
  // EXPECT_EQ(clientPosition, frame.clientPosition_);
}

TEST(FrameTest, Frame_RESUME_OK) {
  FrameFlags flags = FrameFlags::EMPTY_;
  ResumePosition position = 6;
  auto frame = reserialize<Frame_RESUME_OK>(position);

  expectHeader(FrameType::RESUME_OK, flags, 0, frame);
  EXPECT_EQ(position, frame.position_);
}

TEST(FrameTest, Frame_PreallocatedFrameLengthField) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags::COMPLETE;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frameSerializer =
      FrameSerializer::createFrameSerializer(ProtocolVersion::Latest);
  frameSerializer->preallocateFrameSizeField() = true;

  auto frame = Frame_PAYLOAD(streamId, flags, Payload(data->clone()));
  auto serializedFrame = frameSerializer->serializeOut(std::move(frame));

  EXPECT_LT(0, serializedFrame->headroom());
}
