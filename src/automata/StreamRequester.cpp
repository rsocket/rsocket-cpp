// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamRequester.h"

namespace reactivesocket {

void StreamRequester::sendRequestFrame(size_t initialN, Payload&& request) {
  newStream(
      StreamType::STREAM, static_cast<uint32_t>(initialN), std::move(request));
}
}
