// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ResumeCache.h"
#include <algorithm>
#include "src/FrameTransport.h"

namespace reactivesocket {

void ResumeCache::trackSentFrame(const folly::IOBuf& serializedFrame) {
  auto frameType = FrameHeader::peekType(serializedFrame);
  auto streamIdPtr = FrameHeader::peekStreamId(serializedFrame);

  switch (frameType) {
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_STREAM:
    case FrameType::REQUEST_SUB:
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::ERROR:
    case FrameType::RESPONSE:
      // TODO(tmont): this could be expensive, find a better way to determine
      // frame length
      position_ += serializedFrame.computeChainDataLength();

      // TODO(tmont): this is not ideal, but memory usage is more important
      if (streamIdPtr) {
        const StreamId streamId = *streamIdPtr;

        streamMap_[streamId] = position_;
      }

      addFrame(serializedFrame);
      break;

    default:
      break;
  }
}

void ResumeCache::resetUpToPosition(const position_t position) {
  for (auto it = streamMap_.begin(); it != streamMap_.end();) {
    if (it->second <= position) {
      it = streamMap_.erase(it);
    } else {
      it++;
    }
  }

  resetPosition_ = position;

  frames_.erase(
      frames_.begin(),
      std::upper_bound(
          frames_.begin(),
          frames_.end(),
          position,
          [](position_t position, decltype(frames_.back())& pair) {
            return position < pair.first;
          }));
}

bool ResumeCache::isPositionAvailable(position_t position) const {
  return (position == position_) ||
      std::binary_search(
             frames_.begin(),
             frames_.end(),
             std::make_pair(position, std::unique_ptr<folly::IOBuf>()),
             [](decltype(frames_.back())& pairA,
                decltype(frames_.back())& pairB) {
               return pairA.first < pairB.first;
             });
}

bool ResumeCache::isPositionAvailable(position_t position, StreamId streamId)
    const {
  bool result = false;

  auto it = streamMap_.find(streamId);
  if (it != streamMap_.end()) {
    const position_t streamPosition = (*it).second;

    result = (streamPosition <= position);
  } else {
    result = (resetPosition_ >= position);
  }

  return result;
}

void ResumeCache::addFrame(const folly::IOBuf& frame) {
  // TODO: implement bounds to the buffer
  frames_.emplace_back(position_, frame.clone());
}

void ResumeCache::sendFramesFromPosition(
    position_t position,
    FrameTransport& frameTransport) const {
  DCHECK(isPositionAvailable(position));

  if (position == resetPosition_) {
    // idle resumption
    return;
  }

  auto found = std::lower_bound(
      frames_.begin(),
      frames_.end(),
      position,
      [](decltype(frames_.back())& pair, position_t position) {
        return pair.first < position;
      });

  DCHECK(found != frames_.end());
  DCHECK(found->first == position);

  while (++found != frames_.end()) {
    frameTransport.outputFrameOrEnqueue(found->second->clone());
  }
}

} // reactivesocket
