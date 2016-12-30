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
using StreamId = uint32_t;

class StreamState {
 public:
  StreamState()
      : resumeTracker_(new ResumeTracker()), resumeCache_(new ResumeCache()) {}

  virtual ~StreamState() = default;

  std::unordered_map<StreamId, std::shared_ptr<AbstractStreamAutomaton>>
      streams_;
  std::unique_ptr<ResumeTracker> resumeTracker_;
  std::unique_ptr<ResumeCache> resumeCache_;

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
