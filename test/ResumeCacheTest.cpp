// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/IOBuf.h>
#include <folly/Memory.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/Frame.h"
#include "src/FrameTransport.h"
#include "src/ResumeCache.h"
#include "src/Stats.h"
#include "test/InlineConnection.h"

using namespace ::testing;
using namespace ::reactivesocket;

class FrameTransportMock : public FrameTransport {
 public:
  FrameTransportMock() : FrameTransport(folly::make_unique<InlineConnection>()) {}

  MOCK_METHOD1(outputFrameOrEnqueue_, void(std::unique_ptr<folly::IOBuf>&));

  void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame) override {
    outputFrameOrEnqueue_(frame);
  }
};

TEST(ResumeCacheTest, EmptyCache) {
  ResumeCache cache(Stats::noop());
  FrameTransportMock transport;

  EXPECT_CALL(transport, outputFrameOrEnqueue_(_)).Times(0);

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ(0, cache.position());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(1));
  cache.sendFramesFromPosition(0, transport);

  cache.resetUpToPosition(0);

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ(0, cache.position());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(1));
  cache.sendFramesFromPosition(0, transport);
}

TEST(ResumeCacheTest, OneFrame) {
  ResumeCache cache(Stats::noop());
  FrameTransportMock transport;

  auto frame1 = Frame_CANCEL(0).serializeOut();
  const auto frame1Size = frame1->computeChainDataLength();

  cache.trackSentFrame(*frame1);

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ(frame1Size, cache.position());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  cache.resetUpToPosition(0);

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ(frame1Size, cache.position());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  EXPECT_FALSE(cache.isPositionAvailable(frame1Size - 1)); // misaligned

  EXPECT_CALL(transport, outputFrameOrEnqueue_(_))
      .WillOnce(Invoke([=](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame1Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(0, transport);
  cache.sendFramesFromPosition(frame1Size, transport);

  cache.resetUpToPosition(frame1Size);

  EXPECT_EQ(frame1Size, cache.lastResetPosition());
  EXPECT_EQ(frame1Size, cache.position());
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  cache.sendFramesFromPosition(frame1Size, transport);
}

TEST(ResumeCacheTest, TwoFrames) {
  ResumeCache cache(Stats::noop());
  FrameTransportMock transport;

  auto frame1 = Frame_CANCEL(0).serializeOut();
  const auto frame1Size = frame1->computeChainDataLength();
  auto frame2 = Frame_REQUEST_N(0, 0).serializeOut();
  const auto frame2Size = frame2->computeChainDataLength();

  cache.trackSentFrame(*frame1);
  cache.trackSentFrame(*frame2);

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ(frame1Size + frame2Size, cache.position());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size + frame2Size));

  EXPECT_CALL(transport, outputFrameOrEnqueue_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame1Size, buf->computeChainDataLength());
      }))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame2Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(0, transport);

  cache.resetUpToPosition(frame1Size);

  EXPECT_EQ(frame1Size, cache.lastResetPosition());
  EXPECT_EQ(frame1Size + frame2Size, cache.position());
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size + frame2Size));

  EXPECT_CALL(transport, outputFrameOrEnqueue_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame2Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(frame1Size, transport);
}
