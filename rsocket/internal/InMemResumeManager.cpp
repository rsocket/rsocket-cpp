// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/InMemResumeManager.h"

#include <algorithm>

namespace {

using rsocket::FrameType;

bool shouldTrackFrame(const FrameType frameType) {
  switch (frameType) {
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_STREAM:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::ERROR:
    case FrameType::PAYLOAD:
      return true;
    case FrameType::RESERVED:
    case FrameType::SETUP:
    case FrameType::LEASE:
    case FrameType::KEEPALIVE:
    case FrameType::METADATA_PUSH:
    case FrameType::RESUME:
    case FrameType::RESUME_OK:
    case FrameType::EXT:
    default:
      return false;
  }
}

bool isNewStreamFrame(FrameType frameType) {
  return frameType == FrameType::REQUEST_CHANNEL ||
      frameType == FrameType::REQUEST_STREAM ||
      frameType == FrameType::REQUEST_RESPONSE ||
      frameType == FrameType::REQUEST_FNF;
}

} // anonymous

namespace rsocket {

InMemResumeManager::~InMemResumeManager() {
  clearFrames(lastSentPosition_);
}

void InMemResumeManager::trackReceivedFrame(
    const folly::IOBuf& serializedFrame,
    FrameType frameType,
    StreamId streamId) {
  if (isNewStreamFrame(frameType)) {
    onRemoteStreamOpen(streamId, frameType);
  }
  if (shouldTrackFrame(frameType)) {
    VLOG(6) << "received frame " << frameType;
    // TODO(tmont): this could be expensive, find a better way to get length
    impliedPosition_ += serializedFrame.computeChainDataLength();
  }
}

void InMemResumeManager::trackSentFrame(
    const folly::IOBuf& serializedFrame,
    FrameType frameType,
    folly::Optional<StreamId> streamIdPtr) {
  if (streamIdPtr) {
    const StreamId streamId = *streamIdPtr;
    if (isNewStreamFrame(frameType)) {
      onLocalStreamOpen(streamId, frameType);
    }
  }

  if (shouldTrackFrame(frameType)) {
    // TODO(tmont): this could be expensive, find a better way to get length
    auto frameDataLength = serializedFrame.computeChainDataLength();

    // if the frame is too huge, we don't cache it
    if (frameDataLength > capacity_) {
      resetUpToPosition(lastSentPosition_);
      lastSentPosition_ += frameDataLength;
      DCHECK(size_ == 0);
      return;
    }

    addFrame(serializedFrame, frameDataLength);
    lastSentPosition_ += frameDataLength;
  }
}

void InMemResumeManager::resetUpToPosition(ResumePosition position) {
  if (position <= firstSentPosition_) {
    return;
  }

  if (position > lastSentPosition_) {
    position = lastSentPosition_;
  }

  clearFrames(position);

  firstSentPosition_ = position;
  DCHECK(frames_.empty() || frames_.front().first == firstSentPosition_);
}

bool InMemResumeManager::isPositionAvailable(ResumePosition position) const {
  return (lastSentPosition_ == position) ||
      std::binary_search(
             frames_.begin(),
             frames_.end(),
             std::make_pair(position, std::unique_ptr<folly::IOBuf>()),
             [](decltype(frames_.back()) pairA,
                decltype(frames_.back()) pairB) {
               return pairA.first < pairB.first;
             });
}

void InMemResumeManager::addFrame(
    const folly::IOBuf& frame,
    size_t frameDataLength) {
  size_ += frameDataLength;
  while (size_ > capacity_) {
    evictFrame();
  }
  frames_.emplace_back(lastSentPosition_, frame.clone());
  stats_->resumeBufferChanged(1, static_cast<int>(frameDataLength));
}

void InMemResumeManager::evictFrame() {
  DCHECK(!frames_.empty());

  auto position = frames_.size() > 1 ? std::next(frames_.begin())->first
                                     : lastSentPosition_;
  resetUpToPosition(position);
}

void InMemResumeManager::clearFrames(ResumePosition position) {
  if (frames_.empty()) {
    return;
  }
  DCHECK(position <= lastSentPosition_);
  DCHECK(position >= firstSentPosition_);

  auto end = std::lower_bound(
      frames_.begin(),
      frames_.end(),
      position,
      [](decltype(frames_.back()) pair, ResumePosition pos) {
        return pair.first < pos;
      });
  DCHECK(end == frames_.end() || end->first >= firstSentPosition_);
  auto pos = end == frames_.end() ? position : end->first;
  stats_->resumeBufferChanged(
      -static_cast<int>(std::distance(frames_.begin(), end)),
      -static_cast<int>(pos - firstSentPosition_));

  frames_.erase(frames_.begin(), end);
  size_ -= static_cast<decltype(size_)>(pos - firstSentPosition_);
}

void InMemResumeManager::sendFramesFromPosition(
    ResumePosition position,
    FrameTransport& frameTransport) const {
  DCHECK(isPositionAvailable(position));

  if (position == lastSentPosition_) {
    // idle resumption
    return;
  }

  auto found = std::lower_bound(
      frames_.begin(),
      frames_.end(),
      position,
      [](decltype(frames_.back()) pair, ResumePosition pos) {
        return pair.first < pos;
      });

  DCHECK(found != frames_.end());
  DCHECK(found->first == position);

  while (found != frames_.end()) {
    frameTransport.outputFrameOrDrop(found->second->clone());
    found++;
  }
}

void InMemResumeManager::onStreamClosed(StreamId streamId) {
  // This is crude. We could try to preserve the stream type in
  // RSocketStateMachine and pass it down explicitly here.
  requesterRequestStreams_.erase(streamId);
  requesterRequestChannels_.erase(streamId);
  requesterRequestResponses_.erase(streamId);
  responderRequestStreams_.erase(streamId);
  responderRequestChannels_.erase(streamId);
  responderRequestResponses_.erase(streamId);
  publisherAllowance_.erase(streamId);
  consumerAllowance_.erase(streamId);
  streamTokens_.erase(streamId);
}

void InMemResumeManager::onLocalStreamOpen(
    StreamId streamId,
    FrameType frameType) {
  if (streamId > largestUsedStreamId_) {
    largestUsedStreamId_ = streamId;
  }
  if (frameType == FrameType::REQUEST_STREAM) {
    requesterRequestStreams_.insert(streamId);
  } else if (frameType == FrameType::REQUEST_CHANNEL) {
    requesterRequestChannels_.insert(streamId);
  } else if (frameType == FrameType::REQUEST_RESPONSE) {
    requesterRequestResponses_.insert(streamId);
  }
}

void InMemResumeManager::onRemoteStreamOpen(
    StreamId streamId,
    FrameType frameType) {
  if (frameType == FrameType::REQUEST_STREAM) {
    responderRequestStreams_.insert(streamId);
  } else if (frameType == FrameType::REQUEST_CHANNEL) {
    responderRequestChannels_.insert(streamId);
  } else if (frameType == FrameType::REQUEST_RESPONSE) {
    responderRequestResponses_.insert(streamId);
  }
}

std::unordered_set<StreamId>
InMemResumeManager::getRequesterRequestStreamIds() {
  return requesterRequestStreams_;
}

} // reactivesocket
