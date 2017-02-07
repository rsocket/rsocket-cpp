// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ResumeTracker.h"
#include <folly/Optional.h>
#include "src/Frame.h"

namespace reactivesocket {

void ResumeTracker::trackReceivedFrame(const folly::IOBuf& serializedFrame) {
  if (shouldTrackFrame(serializedFrame)) {
    // TODO(tmont): this could be expensive, find a better way to determine
    // frame length
    VLOG(6) << "received frame " << FrameHeader::peekType(serializedFrame);
    impliedPosition_ += serializedFrame.computeChainDataLength();
  }
}

bool ResumeTracker::shouldTrackFrame(const folly::IOBuf& serializedFrame) {
  auto frameType = FrameHeader::peekType(serializedFrame);

  switch (frameType) {
    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_STREAM:
    case FrameType::REQUEST_SUB:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::REQUEST_FNF:
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::ERROR:
    case FrameType::RESPONSE:
      return true;

    case FrameType::RESERVED:
    case FrameType::SETUP:
    case FrameType::LEASE:
    case FrameType::KEEPALIVE:
    case FrameType::METADATA_PUSH:
    case FrameType::RESUME:
    case FrameType::RESUME_OK:
    default:
      return false;
  }
}

} // reactivesocket
