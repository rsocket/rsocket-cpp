// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/ConnectionAutomaton.h"
#include "src/StreamState.h"
#include "test/MockStats.h"

using namespace reactivesocket;
using namespace testing;

class StreamStateTest : public Test {
 protected:
  std::shared_ptr<StrictMock<MockStats>> stats_{
      std::make_shared<StrictMock<MockStats>>()};
  std::shared_ptr<ConnectionAutomaton> automaton_{
      std::make_shared<ConnectionAutomaton>(
          inlineExecutor(),
          nullptr,
          nullptr,
          stats_,
          nullptr,
          ReactiveSocketMode::CLIENT)};
  StreamState state_{*automaton_};
};

TEST_F(StreamStateTest, Stats) {
  auto frame1Size = 7, frame2Size = 11;
  InSequence seq;
  EXPECT_CALL(*stats_, streamBufferChanged(1, frame1Size));
  state_.enqueueOutputPendingFrame(
      folly::IOBuf::copyBuffer(std::string(frame1Size, 'x')));
  EXPECT_CALL(*stats_, streamBufferChanged(1, frame2Size));
  state_.enqueueOutputPendingFrame(
      folly::IOBuf::copyBuffer(std::string(frame2Size, 'x')));
  EXPECT_CALL(*stats_, streamBufferChanged(-2, -(frame1Size + frame2Size)));
  state_.moveOutputPendingFrames();
}

TEST_F(StreamStateTest, StatsUpdatedInDtor) {
  auto frameSize = 7;
  InSequence seq;
  EXPECT_CALL(*stats_, streamBufferChanged(1, frameSize));
  state_.enqueueOutputPendingFrame(
      folly::IOBuf::copyBuffer(std::string(frameSize, 'x')));
  EXPECT_CALL(*stats_, streamBufferChanged(-1, -frameSize));
}
