// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/test_utils/ColdResumeManager.h"

namespace rsocket {

ColdResumeManager::~ColdResumeManager() {}

void ColdResumeManager::trackReceivedFrame(
    size_t frameLength,
    FrameType frameType,
    StreamId streamId,
    size_t consumerAllowance) {
  if (!shouldTrackFrame(frameType)) {
    return;
  }
  auto it = streamResumeInfos_.find(streamId);
  // If streamId is not present in streamResumeInfo it likely means a
  // COMPLETE/CANCEL/ERROR was received in this frame and the
  // ResumeMananger::onCloseStream() was already invoked resulting in the entry
  // being deleted.
  if (it != streamResumeInfos_.end()) {
    it->second.consumerAllowance = consumerAllowance;
  }
  WarmResumeManager::trackReceivedFrame(
      frameLength, frameType, streamId, consumerAllowance);
}

void ColdResumeManager::trackSentFrame(
    const folly::IOBuf& serializedFrame,
    FrameType frameType,
    folly::Optional<StreamId> streamIdPtr,
    size_t consumerAllowance) {
  if (!shouldTrackFrame(frameType)) {
    return;
  }
  if (streamIdPtr) {
    const StreamId streamId = *streamIdPtr;
    auto it = streamResumeInfos_.find(streamId);
    CHECK(it != streamResumeInfos_.end());
    it->second.consumerAllowance = consumerAllowance;
  }
  WarmResumeManager::trackSentFrame(
      std::move(serializedFrame), frameType, streamIdPtr, consumerAllowance);
}

void ColdResumeManager::onStreamClosed(StreamId streamId) {
  streamResumeInfos_.erase(streamId);
}

void ColdResumeManager::onStreamOpen(
    StreamId streamId,
    RequestOriginator requester,
    std::string streamToken,
    StreamType streamType) {
  CHECK(streamType != StreamType::FNF);
  CHECK(streamResumeInfos_.find(streamId) == streamResumeInfos_.end());
  if (requester == RequestOriginator::LOCAL &&
      streamId > largestUsedStreamId_) {
    largestUsedStreamId_ = streamId;
  }
  streamResumeInfos_.emplace(
      streamId, StreamResumeInfo(streamType, requester, streamToken));
}

} // reactivesocket
