// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/framing/Frame.h"
#include "src/framing/FrameTransport.h"
#include "src/temporary_home/ResumeCache.h"
#include "src/temporary_home/Stats.h"
#include "src/framing/FrameSerializer_v0_1.h"
#include "test/test_utils/InlineConnection.h"
#include "test/test_utils/MockStats.h"

using namespace ::testing;
using namespace ::reactivesocket;

class FrameTransportMock : public FrameTransport {
 public:
  FrameTransportMock() : FrameTransport(std::make_unique<InlineConnection>()) {}

  MOCK_METHOD1(outputFrameOrEnqueue_, void(std::unique_ptr<folly::IOBuf>&));

  void outputFrameOrEnqueue(std::unique_ptr<folly::IOBuf> frame) override {
    outputFrameOrEnqueue_(frame);
  }
};

class ResumeCacheTest : public Test {
 protected:
  std::unique_ptr<FrameSerializer> frameSerializer_{
      FrameSerializer::createCurrentVersion()};
};

TEST_F(ResumeCacheTest, EmptyCache) {
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

TEST_F(ResumeCacheTest, OneFrame) {
  ResumeCache cache(Stats::noop());
  FrameTransportMock transport;

  auto frame1 = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frame1Size = frame1->computeChainDataLength();

  cache.trackSentFrame(
      *frame1, FrameType::CANCEL, folly::Optional<StreamId>(0));

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ((ResumePosition)frame1Size, cache.position());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  cache.resetUpToPosition(0);

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ((ResumePosition)frame1Size, cache.position());
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

  EXPECT_EQ((ResumePosition)frame1Size, cache.lastResetPosition());
  EXPECT_EQ((ResumePosition)frame1Size, cache.position());
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  cache.sendFramesFromPosition(frame1Size, transport);
}

TEST_F(ResumeCacheTest, TwoFrames) {
  ResumeCache cache(Stats::noop());
  FrameTransportMock transport;

  auto frame1 = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frame1Size = frame1->computeChainDataLength();

  auto frame2 = frameSerializer_->serializeOut(Frame_REQUEST_N(0, 2));
  const auto frame2Size = frame2->computeChainDataLength();

  cache.trackSentFrame(
      *frame1, FrameType::CANCEL, folly::Optional<StreamId>(0));
  cache.trackSentFrame(
      *frame2, FrameType::REQUEST_N, folly::Optional<StreamId>(0));

  EXPECT_EQ(0, cache.lastResetPosition());
  EXPECT_EQ((ResumePosition)(frame1Size + frame2Size), cache.position());
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

  EXPECT_EQ((ResumePosition)frame1Size, cache.lastResetPosition());
  EXPECT_EQ((ResumePosition)(frame1Size + frame2Size), cache.position());
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size + frame2Size));

  EXPECT_CALL(transport, outputFrameOrEnqueue_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame2Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(frame1Size, transport);
}

TEST_F(ResumeCacheTest, Stats) {
  auto stats = std::make_shared<StrictMock<MockStats>>();
  ResumeCache cache(stats);

  auto frame1 = frameSerializer_->serializeOut(Frame_CANCEL(0));
  auto frame1Size = frame1->computeChainDataLength();
  EXPECT_CALL(*stats, resumeBufferChanged(1, frame1Size));
  cache.trackSentFrame(
      *frame1, FrameType::CANCEL, folly::Optional<StreamId>(0));

  auto frame2 = frameSerializer_->serializeOut(Frame_REQUEST_N(0, 3));
  auto frame2Size = frame2->computeChainDataLength();
  EXPECT_CALL(*stats, resumeBufferChanged(1, frame2Size)).Times(2);
  cache.trackSentFrame(
      *frame2, FrameType::REQUEST_N, folly::Optional<StreamId>(0));
  cache.trackSentFrame(
      *frame2, FrameType::REQUEST_N, folly::Optional<StreamId>(0));

  EXPECT_CALL(*stats, resumeBufferChanged(-1, -frame1Size));
  cache.resetUpToPosition(frame1Size);
  EXPECT_CALL(*stats, resumeBufferChanged(-2, -2 * frame2Size));
}

TEST_F(ResumeCacheTest, EvictFIFO) {
  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // construct cache with capacity of 2 frameSize
  ResumeCache cache(Stats::noop(), frameSize * 2);

  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));

  // first 2 frames should be presented in the cache
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 2));

  // add third frame, and this frame should evict first frame
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  EXPECT_FALSE(cache.isPositionAvailable(0));

  // cache size should also be adjusted by resetUpToPosition
  cache.resetUpToPosition(frameSize * 2);
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 4));

  // create a huge frame and try to cache it
  auto hugeFrame = folly::IOBuf::createChain(frameSize * 3, frameSize * 3);
  for (int i = 0; i < 3; i++) {
    hugeFrame->appendChain(frame->clone());
  }
  EXPECT_EQ(hugeFrame->computeChainDataLength(), frameSize * 3);
  cache.trackSentFrame(
      *hugeFrame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  // cache should be cleared
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4 + 1));

  // caching small frames shouldn't be affected
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4 + 1));
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 5 + 1));
}

TEST_F(ResumeCacheTest, EvictStats) {
  auto stats = std::make_shared<StrictMock<MockStats>>();

  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // construct cache with capacity of 2 frameSize
  ResumeCache cache(stats, frameSize * 2);

  {
    InSequence dummy;
    // Two added
    EXPECT_CALL(*stats, resumeBufferChanged(1, frameSize));
    EXPECT_CALL(*stats, resumeBufferChanged(1, frameSize));
    // One evicted, one added
    EXPECT_CALL(*stats, resumeBufferChanged(-1, -frameSize));
    EXPECT_CALL(*stats, resumeBufferChanged(1, frameSize));
    // Destruction
    EXPECT_CALL(*stats, resumeBufferChanged(-2, -frameSize * 2));
  }

  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));

  EXPECT_EQ(frameSize * 2, cache.size());
}

TEST_F(ResumeCacheTest, PositionSmallFrame) {
  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // Cache is larger than frame
  ResumeCache cache(Stats::noop(), frameSize * 2);
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  EXPECT_EQ(
      frame->computeChainDataLength(), static_cast<size_t>(cache.position()));
}

TEST_F(ResumeCacheTest, PositionLargeFrame) {
  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // Cache is smaller than frame
  ResumeCache cache(Stats::noop(), frameSize / 2);
  cache.trackSentFrame(*frame, FrameType::CANCEL, folly::Optional<StreamId>(0));
  EXPECT_EQ(
      frame->computeChainDataLength(), static_cast<size_t>(cache.position()));
}
