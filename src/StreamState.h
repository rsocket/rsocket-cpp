// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>

#include "src/ResumeCache.h"
#include "src/ResumeTracker.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class AbstractStreamAutomaton;
class Stats;
using StreamId = uint32_t;

class StreamState {
 public:
  explicit StreamState(Stats& stats) : resumeCache_(stats) {}

  std::unordered_map<StreamId, std::shared_ptr<AbstractStreamAutomaton>>
      streams_;
  ResumeTracker resumeTracker_;
  ResumeCache resumeCache_;

  void enqueueOutputPendingFrame(std::unique_ptr<folly::IOBuf> frame) {
    outputFrames_.push_back(std::move(frame));
  }

  std::deque<std::unique_ptr<folly::IOBuf>> moveOutputPendingFrames() {
    return std::move(outputFrames_);
  }

 private:
  std::deque<std::unique_ptr<folly::IOBuf>> outputFrames_;
};
}
