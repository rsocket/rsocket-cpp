// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamRequester.h"

namespace reactivesocket {

void StreamRequester::sendRequestFrame(size_t initialN, Payload&& request) {
  Frame_REQUEST_STREAM frame(
      streamId_,
      FrameFlags::EMPTY,
      static_cast<uint32_t>(initialN),
      std::move(request));
  connection_->outputFrameOrEnqueue(
      connection_->frameSerializer().serializeOut(std::move(frame)));
}
}
