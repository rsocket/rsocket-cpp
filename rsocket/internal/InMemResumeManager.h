// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <deque>
#include <map>
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
      size_t frameLength,
      FrameType frameType,
      StreamId streamId,
      size_t consumerAllowance) override;

  void trackSentFrame(
      const folly::IOBuf& serializedFrame,
      FrameType frameType,
      folly::Optional<StreamId> streamIdPtr,
      size_t consumerAllowance) override;

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

  void onStreamClosed(StreamId streamId) override;

  std::unordered_set<StreamId> getRequesterRequestStreamIds() override;

  size_t getPublisherAllowance(StreamId streamId) override {
    auto it = consumerAllowance_.find(streamId);
    return it != consumerAllowance_.end() ? it->second : 0;
  }

  size_t getConsumerAllowance(StreamId streamId) override {
    auto it = consumerAllowance_.find(streamId);
    return it != consumerAllowance_.end() ? it->second : 0;
  }

  std::string getStreamToken(StreamId streamId) override {
    auto it = streamTokens_.find(streamId);
    return it != streamTokens_.end() ? it->second : "";
  }

  void saveStreamToken(StreamId streamId, std::string streamToken) override {
    streamTokens_[streamId] = streamToken;
  }

  StreamId getLargestUsedStreamId() override {
    return largestUsedStreamId_;
  }

 private:
  void addFrame(const folly::IOBuf&, size_t);
  void evictFrame();

  void onLocalStreamOpen(StreamId streamId, FrameType frameType);
  void onRemoteStreamOpen(StreamId streamId, FrameType frameType);

  // Called before clearing cached frames to update stats.
  void clearFrames(ResumePosition position);

  std::shared_ptr<RSocketStats> stats_;

  // Start position of the send buffer queue
  ResumePosition firstSentPosition_{0};
  // End position of the send buffer queue
  ResumePosition lastSentPosition_{0};
  // Inferred position of the rcvd frames
  ResumePosition impliedPosition_{0};

  // StreamIds of streams originating locally
  std::unordered_set<StreamId> requesterRequestStreams_;
  std::unordered_set<StreamId> requesterRequestChannels_;
  std::unordered_set<StreamId> requesterRequestResponses_;

  // StreamIds of streams originating remotely
  std::unordered_set<StreamId> responderRequestStreams_;
  std::unordered_set<StreamId> responderRequestChannels_;
  std::unordered_set<StreamId> responderRequestResponses_;

  // This stores the allowance for streams where the local side
  // is a publisher (REQUEST_STREAM Responder and REQUEST_CHANNEL)
  std::map<StreamId, size_t> publisherAllowance_;

  // This stores the allowance for streams where the local side
  // is a consumer (REQUEST_STREAM Requester and REQUEST_CHANNEL)
  std::map<StreamId, size_t> consumerAllowance_;

  // This is a mapping between streamIds and application-aware streamTokens
  std::map<StreamId, std::string> streamTokens_;

  // Largest used StreamId so far.
  StreamId largestUsedStreamId_{0};

  std::deque<std::pair<ResumePosition, std::unique_ptr<folly::IOBuf>>> frames_;

  constexpr static size_t DEFAULT_CAPACITY = 1024 * 1024; // 1MB
  const size_t capacity_;
  size_t size_{0};
};
}
