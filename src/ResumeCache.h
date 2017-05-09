// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <unordered_map>

#include <folly/Optional.h>

#include "src/Common.h"
#include "src/Stats.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class ConnectionAutomaton;
class FrameTransport;

// This class stores information necessary to resume the RSocket session.  The
// stored information fall into two categories.  (1) Sent:  Here we have a
// buffer queue of sent frames (limited by capacity).  We have two pointers -
// position_ and resetPosition_, which track the position (in bytes) of the
// first and last frames we have in queue.  (2) Rcvd: We have a
// impliedPosition_ byte counter, which determines the bytes until which we
// have received data from the other side.
class ResumeCache {
 public:
  explicit ResumeCache(
      std::shared_ptr<Stats> stats,
      size_t capacity = DEFAULT_CAPACITY)
      : stats_(std::move(stats)), capacity_(capacity) {}
  ~ResumeCache();

  // Tracks a received frame.
  void trackReceivedFrame(
      const folly::IOBuf& serializedFrame,
      const FrameType frameType);

  // Tracks a sent frame.
  void trackSentFrame(
      const folly::IOBuf& serializedFrame,
      const FrameType frameType,
      const folly::Optional<StreamId> streamIdPtr);

  // Resets the send buffer buffer until the given position.
  // This is triggered on KeepAlive reception or when we hit capacity.
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

  ResumePosition impliedPosition() {
    return impliedPosition_;
  }

  bool canResumeFrom(ResumePosition clientPosition) const {
    return clientPosition <= impliedPosition_;
  }

  size_t size() const {
    return size_;
  }

 private:
  void addFrame(const folly::IOBuf&, size_t);
  void evictFrame();

  // Called before clearing cached frames to update stats.
  void clearFrames(ResumePosition position);

  std::shared_ptr<Stats> stats_;

  // End position of the send buffer queue
  ResumePosition position_{0};
  // Start position of the send buffer queue
  ResumePosition resetPosition_{0};
  // Inferred position of the rcvd frames
  ResumePosition impliedPosition_{0};

  std::unordered_map<StreamId, ResumePosition> streamMap_;

  std::deque<std::pair<ResumePosition, std::unique_ptr<folly::IOBuf>>> frames_;

  constexpr static size_t DEFAULT_CAPACITY = 1024 * 1024; // 1MB
  const size_t capacity_;
  size_t size_{0};
};
}
