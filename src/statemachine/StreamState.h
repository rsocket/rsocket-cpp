// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <stdint.h>
#include <deque>
#include <unordered_map>

#include "src/statemachine/StreamStateMachineBase.h"
#include "yarpl/Refcounted.h"

namespace rsocket {

class RSocketStateMachine;
class RSocketStats;
class StreamStateMachineBase;
using StreamId = uint32_t;

class StreamState {
 public:
  explicit StreamState(RSocketStats& stats);
  ~StreamState();

  void enqueueOutputPendingFrame(std::unique_ptr<folly::IOBuf> frame);

  std::deque<std::unique_ptr<folly::IOBuf>> moveOutputPendingFrames();

  std::unordered_map<StreamId, yarpl::Reference<StreamStateMachineBase>>
      streams_;

 private:
  /// Called to update stats when outputFrames_ is about to be cleared.
  void onClearFrames();

  RSocketStats& stats_;

  /// Total data length of all IOBufs in outputFrames_.
  uint64_t dataLength_{0};

  std::deque<std::unique_ptr<folly::IOBuf>> outputFrames_;
};
}
