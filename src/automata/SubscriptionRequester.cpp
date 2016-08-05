// Copyright 2004-present Facebook. All Rights Reserved.

#include "SubscriptionRequester.h"

#include <algorithm>
#include <iostream>

namespace reactivesocket {

void SubscriptionRequesterBase::sendRequestFrame(
    FrameFlags flags,
    size_t initialN,
    Payload&& request) {
  Frame_REQUEST_SUB frame(
      streamId_,
      flags,
      static_cast<uint32_t>(initialN),
      FrameMetadata::empty(),
      std::move(request));
  connection_->onNextFrame(frame);
}

std::ostream& SubscriptionRequesterBase::logPrefix(std::ostream& os) {
  return os << "SubscriptionRequester(" << &connection_ << ", " << streamId_
            << "): ";
}
}
