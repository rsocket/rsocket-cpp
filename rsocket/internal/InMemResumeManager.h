// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <set>

#include "rsocket/RSocketStats.h"
#include "rsocket/ResumeManager.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

class RSocketStateMachine;
class FrameTransport;

class InMemResumeManager : public ResumeManager {
 public:
  explicit InMemResumeManager(
      std::shared_ptr<RSocketStats> stats,
      size_t capacity = DEFAULT_CAPACITY)
      : stats_(std::move(stats)), capacity_(capacity) {}
  ~InMemResumeManager();

  void trackReceivedFrame(
      const folly::IOBuf& serializedFrame,
      FrameType frameType,
      StreamId streamId) override;

  void trackSentFrame(
      const folly::IOBuf& serializedFrame,
      FrameType frameType,
      folly::Optional<StreamId> streamIdPtr) override;

  void resetUpToPosition(ResumePosition position) override;

  bool isPositionAvailable(ResumePosition position) const override;

  void sendFramesFromPosition(
      ResumePosition position,
      FrameTransport& transport) const override;

  ResumePosition firstSentPosition() const override {
    return firstSentPosition_;
  }

  ResumePosition lastSentPosition() const override {
    return lastSentPosition_;
  }

  ResumePosition impliedPosition() const override {
    return impliedPosition_;
  }

  void onStreamOpen(StreamId streamId, FrameType frameType) override;

  void onStreamClosed(StreamId streamId) override;

 private:
  void addFrame(const folly::IOBuf&, size_t);
  void evictFrame();

  // Called before clearing cached frames to update stats.
  void clearFrames(ResumePosition position);

  std::shared_ptr<RSocketStats> stats_;

  // Start position of the send buffer queue
  ResumePosition firstSentPosition_{0};
  // End position of the send buffer queue
  ResumePosition lastSentPosition_{0};
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
