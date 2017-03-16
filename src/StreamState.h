// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <stdint.h>
#include <deque>
#include <memory>
#include <unordered_map>

#include "src/ResumeCache.h"
#include "src/ResumeTracker.h"

namespace reactivesocket {

class ConnectionAutomaton;
class Stats;
class StreamAutomatonBase;
using StreamId = uint32_t;

class StreamState {
 public:
  explicit StreamState(ConnectionAutomaton& connection);
  ~StreamState();

  void enqueueOutputPendingFrame(std::unique_ptr<folly::IOBuf> frame);

  std::deque<std::unique_ptr<folly::IOBuf>> moveOutputPendingFrames();

  std::unordered_map<StreamId, std::shared_ptr<StreamAutomatonBase>> streams_;
  ResumeTracker resumeTracker_;
  ResumeCache resumeCache_;

 private:
  /// Called to update stats when outputFrames_ is about to be cleared.
  void onClearFrames();

  Stats& stats_;

  /// Total data length of all IOBufs in outputFrames_.
  uint64_t dataLength_{0};

  std::deque<std::unique_ptr<folly::IOBuf>> outputFrames_;
};
}
