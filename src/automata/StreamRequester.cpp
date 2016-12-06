// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamRequester.h"

namespace reactivesocket {

void StreamRequesterBase::sendRequestFrame(
    FrameFlags flags,
    size_t initialN,
    Payload&& request) {
  Frame_REQUEST_STREAM frame(
      streamId_, flags, static_cast<uint32_t>(initialN), std::move(request));
  connection_->outputFrameOrEnqueue(frame.serializeOut());
}

std::ostream& StreamRequesterBase::logPrefix(std::ostream& os) {
  return os << "StreamRequester(" << &connection_ << ", " << streamId_ << "): ";
}
}
