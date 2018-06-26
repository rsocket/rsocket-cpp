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

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>

#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FrameSerializer.h"
#include "rsocket/framing/FrameTransportImpl.h"
#include "rsocket/internal/WarmResumeManager.h"
#include "rsocket/test/test_utils/MockDuplexConnection.h"
#include "rsocket/test/test_utils/MockStats.h"

using namespace ::testing;
using namespace ::rsocket;

namespace {

class FrameTransportMock : public FrameTransportImpl {
 public:
  FrameTransportMock()
      : FrameTransportImpl(std::make_unique<MockDuplexConnection>()) {}

  MOCK_METHOD1(outputFrameOrDrop_, void(std::unique_ptr<folly::IOBuf>&));

  void outputFrameOrDrop(std::unique_ptr<folly::IOBuf> frame) override {
    outputFrameOrDrop_(frame);
  }
};

} // namespace

class WarmResumeManagerTest : public Test {
 protected:
  std::unique_ptr<FrameSerializer> frameSerializer_{
      FrameSerializer::createFrameSerializer(ProtocolVersion(1, 0))};
};

TEST_F(WarmResumeManagerTest, EmptyCache) {
  WarmResumeManager cache(RSocketStats::noop());
  FrameTransportMock transport;

  EXPECT_CALL(transport, outputFrameOrDrop_(_)).Times(0);

  EXPECT_EQ(0, cache.firstSentPosition());
  EXPECT_EQ(0, cache.lastSentPosition());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(1));
  cache.sendFramesFromPosition(0, transport);

  cache.resetUpToPosition(0);

  EXPECT_EQ(0, cache.firstSentPosition());
  EXPECT_EQ(0, cache.lastSentPosition());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(1));
  cache.sendFramesFromPosition(0, transport);
}

TEST_F(WarmResumeManagerTest, OneFrame) {
  WarmResumeManager cache(RSocketStats::noop());
  FrameTransportMock transport;

  auto frame1 = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frame1Size = frame1->computeChainDataLength();

  cache.trackSentFrame(*frame1, FrameType::CANCEL, 1, 0);

  EXPECT_EQ(0, cache.firstSentPosition());
  EXPECT_EQ((ResumePosition)frame1Size, cache.lastSentPosition());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  cache.resetUpToPosition(0);

  EXPECT_EQ(0, cache.firstSentPosition());
  EXPECT_EQ((ResumePosition)frame1Size, cache.lastSentPosition());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  EXPECT_FALSE(cache.isPositionAvailable(frame1Size - 1)); // misaligned

  EXPECT_CALL(transport, outputFrameOrDrop_(_))
      .WillOnce(Invoke([=](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame1Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(0, transport);
  cache.sendFramesFromPosition(frame1Size, transport);

  cache.resetUpToPosition(frame1Size);

  EXPECT_EQ((ResumePosition)frame1Size, cache.firstSentPosition());
  EXPECT_EQ((ResumePosition)frame1Size, cache.lastSentPosition());
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));

  cache.sendFramesFromPosition(frame1Size, transport);
}

TEST_F(WarmResumeManagerTest, TwoFrames) {
  WarmResumeManager cache(RSocketStats::noop());
  FrameTransportMock transport;

  auto frame1 = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frame1Size = frame1->computeChainDataLength();

  auto frame2 = frameSerializer_->serializeOut(Frame_REQUEST_N(0, 2));
  const auto frame2Size = frame2->computeChainDataLength();

  cache.trackSentFrame(*frame1, FrameType::CANCEL, 1, 0);
  cache.trackSentFrame(*frame2, FrameType::REQUEST_N, 1, 0);

  EXPECT_EQ(0, cache.firstSentPosition());
  EXPECT_EQ(
      (ResumePosition)(frame1Size + frame2Size), cache.lastSentPosition());
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size + frame2Size));

  EXPECT_CALL(transport, outputFrameOrDrop_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame1Size, buf->computeChainDataLength());
      }))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame2Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(0, transport);

  cache.resetUpToPosition(frame1Size);

  EXPECT_EQ((ResumePosition)frame1Size, cache.firstSentPosition());
  EXPECT_EQ(
      (ResumePosition)(frame1Size + frame2Size), cache.lastSentPosition());
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size));
  EXPECT_TRUE(cache.isPositionAvailable(frame1Size + frame2Size));

  EXPECT_CALL(transport, outputFrameOrDrop_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& buf) {
        EXPECT_EQ(frame2Size, buf->computeChainDataLength());
      }));

  cache.sendFramesFromPosition(frame1Size, transport);
}

TEST_F(WarmResumeManagerTest, Stats) {
  auto stats = std::make_shared<StrictMock<MockStats>>();
  WarmResumeManager cache(stats);

  auto frame1 = frameSerializer_->serializeOut(Frame_CANCEL(0));
  auto frame1Size = frame1->computeChainDataLength();
  EXPECT_CALL(*stats, resumeBufferChanged(1, frame1Size));
  cache.trackSentFrame(*frame1, FrameType::CANCEL, 1, 0);

  auto frame2 = frameSerializer_->serializeOut(Frame_REQUEST_N(0, 3));
  auto frame2Size = frame2->computeChainDataLength();
  EXPECT_CALL(*stats, resumeBufferChanged(1, frame2Size)).Times(2);
  cache.trackSentFrame(*frame2, FrameType::REQUEST_N, 1, 0);
  cache.trackSentFrame(*frame2, FrameType::REQUEST_N, 1, 0);

  EXPECT_CALL(*stats, resumeBufferChanged(-1, -frame1Size));
  cache.resetUpToPosition(frame1Size);
  EXPECT_CALL(*stats, resumeBufferChanged(-2, -2 * frame2Size));
}

TEST_F(WarmResumeManagerTest, EvictFIFO) {
  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // construct cache with capacity of 2 frameSize
  WarmResumeManager cache(RSocketStats::noop(), frameSize * 2);

  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);

  // first 2 frames should be present in the cache
  EXPECT_TRUE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 2));

  // add third frame, and this frame should evict first frame
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 3));

  // cache size should also be adjusted by resetUpToPosition
  cache.resetUpToPosition(frameSize * 2);
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 3));

  // add fourth frame, this should evict second frame
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 4));

  // create a huge frame and try to cache it
  auto hugeFrame = folly::IOBuf::createChain(frameSize * 3, frameSize * 3);
  for (int i = 0; i < 3; i++) {
    hugeFrame->appendChain(frame->clone());
  }
  auto hugeFrameSize = hugeFrame->computeChainDataLength();
  EXPECT_EQ(hugeFrameSize, frameSize * 3);
  cache.trackSentFrame(*hugeFrame, FrameType::CANCEL, 1, 0);

  // cache should be cleared
  EXPECT_EQ(cache.size(), (size_t)0);
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 4 + hugeFrameSize));
  EXPECT_EQ(
      (ResumePosition)(frameSize * 4 + hugeFrameSize),
      cache.firstSentPosition());
  EXPECT_EQ(
      (ResumePosition)(frameSize * 4 + hugeFrameSize),
      cache.lastSentPosition());

  // caching small frames shouldn't be affected
  // Adding one small frame to cache
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_EQ(cache.size(), frameSize);
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 4 + hugeFrameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 5 + hugeFrameSize));
  EXPECT_EQ(
      (ResumePosition)(frameSize * 4 + hugeFrameSize),
      cache.firstSentPosition());
  EXPECT_EQ(
      (ResumePosition)(frameSize * 5 + hugeFrameSize),
      cache.lastSentPosition());

  // Adding second small frame to cache
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_EQ(cache.size(), frameSize * 2);
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 4 + hugeFrameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 5 + hugeFrameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 6 + hugeFrameSize));
  EXPECT_EQ(
      (ResumePosition)(frameSize * 4 + hugeFrameSize),
      cache.firstSentPosition());
  EXPECT_EQ(
      (ResumePosition)(frameSize * 6 + hugeFrameSize),
      cache.lastSentPosition());

  // Adding third small frame to cache.  Should result in first frame eviction
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_EQ(cache.size(), frameSize * 2);
  EXPECT_FALSE(cache.isPositionAvailable(0));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 2));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 3));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4));
  EXPECT_FALSE(cache.isPositionAvailable(frameSize * 4 + hugeFrameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 5 + hugeFrameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 6 + hugeFrameSize));
  EXPECT_TRUE(cache.isPositionAvailable(frameSize * 7 + hugeFrameSize));
  EXPECT_EQ(
      (ResumePosition)(frameSize * 5 + hugeFrameSize),
      cache.firstSentPosition());
  EXPECT_EQ(
      (ResumePosition)(frameSize * 7 + hugeFrameSize),
      cache.lastSentPosition());
}

TEST_F(WarmResumeManagerTest, EvictStats) {
  auto stats = std::make_shared<StrictMock<MockStats>>();

  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // construct cache with capacity of 2 frameSize
  WarmResumeManager cache(stats, frameSize * 2);

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

  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);

  EXPECT_EQ(frameSize * 2, cache.size());
}

TEST_F(WarmResumeManagerTest, PositionSmallFrame) {
  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // Cache is larger than frame
  WarmResumeManager cache(RSocketStats::noop(), frameSize * 2);
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_EQ(
      frame->computeChainDataLength(),
      static_cast<size_t>(cache.lastSentPosition()));
}

TEST_F(WarmResumeManagerTest, PositionLargeFrame) {
  auto frame = frameSerializer_->serializeOut(Frame_CANCEL(0));
  const auto frameSize = frame->computeChainDataLength();

  // Cache is smaller than frame
  WarmResumeManager cache(RSocketStats::noop(), frameSize / 2);
  cache.trackSentFrame(*frame, FrameType::CANCEL, 1, 0);
  EXPECT_EQ(
      frame->computeChainDataLength(),
      static_cast<size_t>(cache.lastSentPosition()));
}
