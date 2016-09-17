// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ResumeCache.h"

namespace reactivesocket {

bool ResumeCache::retransmitFromPosition(
    position_t initialPosition,
    ConnectionAutomaton& connection,
    const RetransmitFilter& filter) {
  return true;
}
}
