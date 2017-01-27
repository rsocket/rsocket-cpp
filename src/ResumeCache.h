// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <unordered_map>
#include "src/Common.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class FrameTransport;
class Stats;

class ResumeCache {
 public:
  explicit ResumeCache(Stats& stats) : stats_(stats) {}

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

  Stats& stats_;
  ResumePosition position_{0};
  ResumePosition resetPosition_{0};
  std::unordered_map<StreamId, ResumePosition> streamMap_;

  std::deque<std::pair<ResumePosition, std::unique_ptr<folly::IOBuf>>> frames_;
};
}
