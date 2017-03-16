// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/StreamState.h"

#include "src/ConnectionAutomaton.h"
#include "src/Stats.h"

namespace reactivesocket {

StreamState::StreamState(ConnectionAutomaton& connection)
    : resumeTracker_(connection),
      resumeCache_(connection),
      stats_(connection.stats()) {}

StreamState::~StreamState() {
  onClearFrames();
}

void StreamState::enqueueOutputPendingFrame(
    std::unique_ptr<folly::IOBuf> frame) {
  auto length = frame->computeChainDataLength();
  stats_.streamBufferChanged(1, static_cast<int64_t>(length));
  dataLength_ += length;
  outputFrames_.push_back(std::move(frame));
}

std::deque<std::unique_ptr<folly::IOBuf>>
StreamState::moveOutputPendingFrames() {
  onClearFrames();
  return std::move(outputFrames_);
}

void StreamState::onClearFrames() {
  auto numFrames = outputFrames_.size();
  if (numFrames != 0) {
    stats_.streamBufferChanged(
        -static_cast<int64_t>(numFrames), -static_cast<int64_t>(dataLength_));
    dataLength_ = 0;
  }
}
}
