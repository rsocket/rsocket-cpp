// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <stdint.h>
#include <deque>
#include <unordered_map>
#include "src/automata/StreamAutomatonBase.h"
#include "yarpl/Refcounted.h"

namespace reactivesocket {

class ConnectionAutomaton;
class Stats;
class StreamAutomatonBase;
using StreamId = uint32_t;

class StreamState {
 public:
  explicit StreamState(Stats& stats);
  ~StreamState();

  void enqueueOutputPendingFrame(std::unique_ptr<folly::IOBuf> frame);

  std::deque<std::unique_ptr<folly::IOBuf>> moveOutputPendingFrames();

  std::unordered_map<StreamId, yarpl::Reference<StreamAutomatonBase>> streams_;

 private:
  /// Called to update stats when outputFrames_ is about to be cleared.
  void onClearFrames();

  Stats& stats_;

  /// Total data length of all IOBufs in outputFrames_.
  uint64_t dataLength_{0};

  std::deque<std::unique_ptr<folly::IOBuf>> outputFrames_;
};
}
