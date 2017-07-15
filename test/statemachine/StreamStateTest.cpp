// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rsocket/statemachine/StreamState.h"
#include "test/test_utils/MockStats.h"

using namespace rsocket;
using namespace testing;

class StreamStateTest : public Test {
 protected:
  StrictMock<MockStats> stats_;
  StreamState state_{stats_};
};

TEST_F(StreamStateTest, Stats) {
  auto frame1Size = 7, frame2Size = 11;
  EXPECT_CALL(stats_, streamBufferChanged(1, frame1Size));
  state_.enqueueOutputPendingFrame(
      folly::IOBuf::copyBuffer(std::string(frame1Size, 'x')));
  EXPECT_CALL(stats_, streamBufferChanged(1, frame2Size));
  state_.enqueueOutputPendingFrame(
      folly::IOBuf::copyBuffer(std::string(frame2Size, 'x')));
  EXPECT_CALL(stats_, streamBufferChanged(-2, -(frame1Size + frame2Size)));
  state_.moveOutputPendingFrames();
}

TEST_F(StreamStateTest, StatsUpdatedInDtor) {
  auto frameSize = 7;
  EXPECT_CALL(stats_, streamBufferChanged(1, frameSize));
  state_.enqueueOutputPendingFrame(
      folly::IOBuf::copyBuffer(std::string(frameSize, 'x')));
  EXPECT_CALL(stats_, streamBufferChanged(-1, -frameSize));
}
