// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ResumeTracker.h"
#include <folly/Optional.h>
#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"

namespace reactivesocket {

void ResumeTracker::trackReceivedFrame(const folly::IOBuf& serializedFrame) {
  if (shouldTrackFrame(serializedFrame, connection_.frameSerializer())) {
    // TODO(tmont): this could be expensive, find a better way to determine
    // frame length
    VLOG(6) << "received frame "
            << connection_.frameSerializer().peekFrameType(serializedFrame);
    impliedPosition_ += serializedFrame.computeChainDataLength();
  }
}

bool ResumeTracker::shouldTrackFrame(
    const folly::IOBuf& serializedFrame,
    FrameSerializer& frameSerializer) {
  auto frameType = frameSerializer.peekFrameType(serializedFrame);

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

} // reactivesocket
