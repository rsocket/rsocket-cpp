// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/WarmResumeManager.h"

#include <algorithm>

namespace rsocket {

WarmResumeManager::~WarmResumeManager() {
  clearFrames(lastSentPosition_);
}

void WarmResumeManager::trackReceivedFrame(
    size_t frameLength,
    FrameType frameType,
    StreamId streamId,
    size_t consumerAllowance) {
  if (shouldTrackFrame(frameType)) {
    VLOG(6) << "Track received frame " << frameType << " StreamId: " << streamId
            << " Allowance: " << consumerAllowance;
    impliedPosition_ += frameLength;
  }
}

void WarmResumeManager::trackSentFrame(
    const folly::IOBuf& serializedFrame,
    FrameType frameType,
    StreamId,
    size_t consumerAllowance) {
  if (shouldTrackFrame(frameType)) {
    // TODO(tmont): this could be expensive, find a better way to get length
    auto frameDataLength = serializedFrame.computeChainDataLength();

    VLOG(6) << "Track sent frame " << frameType
            << " Allowance: " << consumerAllowance;
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

void WarmResumeManager::resetUpToPosition(ResumePosition position) {
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

bool WarmResumeManager::isPositionAvailable(ResumePosition position) const {
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

void WarmResumeManager::addFrame(
    const folly::IOBuf& frame,
    size_t frameDataLength) {
  size_ += frameDataLength;
  while (size_ > capacity_) {
    evictFrame();
  }
  frames_.emplace_back(lastSentPosition_, frame.clone());
  stats_->resumeBufferChanged(1, static_cast<int>(frameDataLength));
}

void WarmResumeManager::evictFrame() {
  DCHECK(!frames_.empty());

  auto position = frames_.size() > 1 ? std::next(frames_.begin())->first
                                     : lastSentPosition_;
  resetUpToPosition(position);
}

void WarmResumeManager::clearFrames(ResumePosition position) {
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

void WarmResumeManager::sendFramesFromPosition(
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

} // reactivesocket
