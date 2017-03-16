// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <unordered_map>
#include "src/Common.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class ConnectionAutomaton;
class FrameTransport;

class ResumeCache {
 public:
  explicit ResumeCache(
      ConnectionAutomaton& connection,
      size_t capacity = DEFAULT_CAPACITY)
      : connection_(connection), capacity_(capacity) {}
  ~ResumeCache();

  void trackSentFrame(const folly::IOBuf& serializedFrame);

  // called to clear up to a certain position from the cache (from keepalive or
  // resuming)
  void resetUpToPosition(ResumePosition position);

  bool isPositionAvailable(ResumePosition position) const;

  bool isPositionAvailable(ResumePosition position, StreamId streamId) const;

  void sendFramesFromPosition(
      ResumePosition position,
      FrameTransport& transport) const;

  ResumePosition lastResetPosition() const {
    return resetPosition_;
  }

  ResumePosition position() const {
    return position_;
  }

 private:
  void addFrame(const folly::IOBuf&, size_t);
  void evictFrame();
  /// Called before clearing cached frames to update stats.
  void clearFrames(ResumePosition position);

  ConnectionAutomaton& connection_;

  ResumePosition position_{0};
  ResumePosition resetPosition_{0};
  std::unordered_map<StreamId, ResumePosition> streamMap_;

  std::deque<std::pair<ResumePosition, std::unique_ptr<folly::IOBuf>>> frames_;

  // default capacity: 1MB
  constexpr static size_t DEFAULT_CAPACITY = 1024 * 1024;
  const size_t capacity_ = DEFAULT_CAPACITY;
  size_t size_{0};
};
}
