// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FrameFlags.h"

#include <bitset>
#include <ostream>

namespace rsocket {

std::ostream& operator<<(std::ostream& os, FrameFlags flags) {
  return os << std::bitset<16>{raw(flags)};
}
}
