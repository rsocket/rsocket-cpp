// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/internal/WarmResumeManager.h"

namespace folly {
class IOBuf;
}

namespace rsocket {

class RSocketStateMachine;
class FrameTransport;

// In-memory ResumeManager for cold-resumption (for prototyping and
// testing purposes)
class ColdResumeManager : public WarmResumeManager {
 public:
  explicit ColdResumeManager(std::shared_ptr<RSocketStats> stats)
      : WarmResumeManager(std::move(stats)) {}
  ~ColdResumeManager();

  void trackReceivedFrame(
      size_t frameLength,
      FrameType frameType,
      StreamId streamId,
      size_t consumerAllowance) override;

  void trackSentFrame(
      const folly::IOBuf& serializedFrame,
      FrameType frameType,
      folly::Optional<StreamId> streamIdPtr,
      size_t consumerAllowance) override;

  void onStreamOpen(
      StreamId,
      RequestOriginator,
      std::string streamToken,
      StreamType) override;

  void onStreamClosed(StreamId streamId) override;

  const StreamResumeInfos& getStreamResumeInfos() override {
    return streamResumeInfos_;
  }

  StreamId getLargestUsedStreamId() override {
    return largestUsedStreamId_;
  }

 private:
  StreamResumeInfos streamResumeInfos_;

  // Largest used StreamId so far.
  StreamId largestUsedStreamId_{0};
};
}
