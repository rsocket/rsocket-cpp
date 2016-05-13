// Copyright 2004-present Facebook. All Rights Reserved.

#include <utility>

#include <folly/Singleton.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "reactivesocket-cpp/src/Frame.h"
#include "reactivesocket-cpp/src/Payload.h"

using namespace ::testing;
using namespace ::lithium::reactivesocket;

class FrameTest : public ::testing::Test {
  void SetUp() override {
    EXPECT_CALL(allocator, allocateBuffer_(_))
        .WillOnce(Invoke([](size_t size) {
          auto buf = folly::IOBuf::createCombined(size + sizeof(int32_t));
          // create some wasted headroom
          buf->advance(sizeof(int32_t));
          return buf.release();
        }));

    folly::Singleton<lithium::reactivesocket::FrameBufferAllocator>::make_mock(
        [&] { return &allocator; }, /* no tear down */ [](void*) {});
  }

  void TearDown() override {
    folly::SingletonVault::singleton()->destroyInstances();
    // Bring the default allocator.
    folly::Singleton<lithium::reactivesocket::FrameBufferAllocator>::make_mock(
        nullptr);
    folly::SingletonVault::singleton()->reenableInstances();
  }

  class MockBufferAllocator : public FrameBufferAllocator {
   public:
    MOCK_METHOD1(allocateBuffer_, folly::IOBuf*(size_t size));

    std::unique_ptr<folly::IOBuf> allocateBuffer(size_t size) override {
      return std::unique_ptr<folly::IOBuf>(allocateBuffer_(size));
    }
  } allocator;
};

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

TEST_F(FrameTest, Frame_REQUEST_CHANNEL) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_COMPLETE | FrameFlags_REQN_PRESENT;
  uint32_t requestN = 3;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_REQUEST_CHANNEL>(
      streamId, flags, requestN, data->clone());

  expectHeader(FrameType::REQUEST_CHANNEL, flags, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.data_));
}

TEST_F(FrameTest, Frame_REQUEST_N) {
  uint32_t streamId = 42;
  uint32_t requestN = 24;
  auto frame = reserialize<Frame_REQUEST_N>(streamId, requestN);

  expectHeader(FrameType::REQUEST_N, FrameFlags_EMPTY, streamId, frame);
  EXPECT_EQ(requestN, frame.requestN_);
}

TEST_F(FrameTest, Frame_CANCEL) {
  uint32_t streamId = 42;
  auto frame = reserialize<Frame_CANCEL>(streamId);

  expectHeader(FrameType::CANCEL, FrameFlags_EMPTY, streamId, frame);
}

TEST_F(FrameTest, Frame_RESPONE) {
  uint32_t streamId = 42;
  FrameFlags flags = FrameFlags_COMPLETE;
  auto data = folly::IOBuf::copyBuffer("424242");
  auto frame = reserialize<Frame_RESPONSE>(streamId, flags, data->clone());

  expectHeader(FrameType::RESPONSE, flags, streamId, frame);
  EXPECT_TRUE(folly::IOBufEqual()(*data, *frame.data_));
}

TEST_F(FrameTest, Frame_ERROR) {
  uint32_t streamId = 42;
  auto errorCode = ErrorCode::REJECTED;
  auto frame = reserialize<Frame_ERROR>(streamId, errorCode);

  expectHeader(FrameType::ERROR, FrameFlags_EMPTY, streamId, frame);
  EXPECT_EQ(errorCode, frame.errorCode_);
}
