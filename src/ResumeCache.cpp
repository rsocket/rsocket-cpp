// Copyright 2004-present Facebook. All Rights Reserved.
#include "src/ResumeCache.h"
#include <folly/Optional.h>
#include <algorithm>
#include "src/ConnectionAutomaton.h"
#include "src/FrameTransport.h"
#include "src/ResumeTracker.h"
#include "src/Stats.h"

namespace reactivesocket {

ResumeCache::~ResumeCache() {
  clearFrames(position_);
}

void ResumeCache::trackSentFrame(const folly::IOBuf& serializedFrame) {
  if (ResumeTracker::shouldTrackFrame(
          serializedFrame, connection_.frameSerializer())) {
    // TODO(tmont): this could be expensive, find a better way to determine
    auto frameDataLength = serializedFrame.computeChainDataLength();

    // if the frame is too huge, we don't cache it
    if (frameDataLength > capacity_) {
      resetUpToPosition(position_);
      position_ += frameDataLength;
      DCHECK(size_ == 0);
      return;
    }

    addFrame(serializedFrame, frameDataLength);

    position_ += frameDataLength;

    // TODO(tmont): this is not ideal, but memory usage is more important
    auto streamIdPtr =
        connection_.frameSerializer().peekStreamId(serializedFrame);
    if (streamIdPtr) {
      const StreamId streamId = *streamIdPtr;

      streamMap_[streamId] = position_;
    }
  }
}

void ResumeCache::resetUpToPosition(ResumePosition position) {
  if (position <= resetPosition_) {
    return;
  }

  if (position > position_) {
    position = position_;
  }

  for (auto it = streamMap_.begin(); it != streamMap_.end();) {
    if (it->second <= position) {
      it = streamMap_.erase(it);
    } else {
      it++;
    }
  }

  clearFrames(position);

  resetPosition_ = position;
  DCHECK(frames_.empty() || frames_.front().first == resetPosition_);
}

bool ResumeCache::isPositionAvailable(ResumePosition position) const {
  return (position_ == position) ||
      std::binary_search(
             frames_.begin(),
             frames_.end(),
             std::make_pair(position, std::unique_ptr<folly::IOBuf>()),
             [](decltype(frames_.back()) pairA,
                decltype(frames_.back()) pairB) {
               return pairA.first < pairB.first;
             });
}

bool ResumeCache::isPositionAvailable(
    ResumePosition position,
    StreamId streamId) const {
  bool result = false;

  auto it = streamMap_.find(streamId);
  if (it != streamMap_.end()) {
    const ResumePosition streamPosition = (*it).second;

    result = (streamPosition <= position);
  } else {
    result = (resetPosition_ >= position);
  }

  return result;
}

void ResumeCache::addFrame(const folly::IOBuf& frame, size_t frameDataLength) {
  size_ += frameDataLength;
  while (size_ > capacity_) {
    evictFrame();
  }
  frames_.emplace_back(position_, frame.clone());
  connection_.stats().resumeBufferChanged(1, static_cast<int>(frameDataLength));
}

void ResumeCache::evictFrame() {
  DCHECK(!frames_.empty());

  auto position =
      frames_.size() > 1 ? std::next(frames_.begin())->first : position_;
  resetUpToPosition(position);
}

void ResumeCache::clearFrames(ResumePosition position) {
  if (frames_.empty()) {
    return;
  }
  DCHECK(position <= position_);
  DCHECK(position >= resetPosition_);

  auto end = std::lower_bound(
      frames_.begin(),
      frames_.end(),
      position,
      [](decltype(frames_.back()) pair, ResumePosition pos) {
        return pair.first < pos;
      });
  DCHECK(end == frames_.end() || end->first >= resetPosition_);
  auto pos = end == frames_.end() ? position : end->first;
  connection_.stats().resumeBufferChanged(
      -static_cast<int>(std::distance(frames_.begin(), end)),
      -static_cast<int>(pos - resetPosition_));

  frames_.erase(frames_.begin(), end);
  size_ -= static_cast<decltype(size_)>(pos - resetPosition_);
}

void ResumeCache::sendFramesFromPosition(
    ResumePosition position,
    FrameTransport& frameTransport) const {
  DCHECK(isPositionAvailable(position));

  if (position == position_) {
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
    frameTransport.outputFrameOrEnqueue(found->second->clone());
    found++;
  }
}

} // reactivesocket
