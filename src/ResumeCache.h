// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Optional.h>
#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>
#include <unordered_map>
#include "src/Frame.h"

namespace reactivesocket {

class FrameTransport;

class ResumeCache {
 public:
  using position_t = ResumePosition;

  ResumeCache() : position_(0), resetPosition_(0) {}

  void trackSentFrame(const folly::IOBuf& serializedFrame);

  // called to clear up to a certain position from the cache (from keepalive or
  // resuming)
  void resetUpToPosition(const position_t position);

  bool isPositionAvailable(position_t position) const;

  bool isPositionAvailable(position_t position, StreamId streamId) const;

  void sendFramesFromPosition(position_t position, FrameTransport& transport)
      const;

 private:
  void addFrame(const folly::IOBuf&);

  position_t position() const {
    return position_;
  }

  position_t position_;
  position_t resetPosition_;
  std::unordered_map<StreamId, position_t> streamMap_;

  std::deque<std::pair<position_t, std::unique_ptr<folly::IOBuf>>> frames_;
};
}
