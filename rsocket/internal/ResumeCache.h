// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <set>
#include <unordered_map>

#include <folly/Optional.h>

#include "rsocket/RSocketStats.h"
#include "rsocket/internal/Common.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

class RSocketStateMachine;
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
      std::shared_ptr<RSocketStats> stats,
      size_t capacity = DEFAULT_CAPACITY)
      : stats_(std::move(stats)), capacity_(capacity) {}
  ~ResumeCache();

  // Tracks a received frame.
  void trackReceivedFrame(
      const folly::IOBuf& serializedFrame,
      const FrameType frameType,
      const StreamId streamId);

  // Tracks a sent frame.
  void trackSentFrame(
      const folly::IOBuf& serializedFrame,
      const FrameType frameType,
      const folly::Optional<StreamId> streamIdPtr);

  // Resets the send buffer buffer until the given position.
  // This is triggered on KeepAlive reception or when we hit capacity.
  void resetUpToPosition(ResumePosition position);

  bool isPositionAvailable(ResumePosition position) const;

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

  void onStreamOpen(StreamId streamId, FrameType frameType);

  void onStreamClosed(StreamId streamId);

 private:
  void addFrame(const folly::IOBuf&, size_t);
  void evictFrame();

  // Called before clearing cached frames to update stats.
  void clearFrames(ResumePosition position);

  std::shared_ptr<RSocketStats> stats_;

  // End position of the send buffer queue
  ResumePosition position_{0};
  // Start position of the send buffer queue
  ResumePosition resetPosition_{0};
  // Inferred position of the rcvd frames
  ResumePosition impliedPosition_{0};

  // Active REQUEST_STREAMs are preserved here
  std::set<StreamId> activeRequestStreams_;

  // Active REQUEST_CHANNELs are preserved here
  std::set<StreamId> activeRequestChannels_;

  // Active REQUEST_RESPONSEs are preserved here
  std::set<StreamId> activeRequestResponses_;

  std::deque<std::pair<ResumePosition, std::unique_ptr<folly::IOBuf>>> frames_;

  constexpr static size_t DEFAULT_CAPACITY = 1024 * 1024; // 1MB
  const size_t capacity_;
  size_t size_{0};
};
}
