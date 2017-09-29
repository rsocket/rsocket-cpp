// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "rsocket/framing/FrameFlags.h"
#include "rsocket/framing/FrameType.h"
#include "rsocket/internal/Common.h"

namespace rsocket {

class FrameHeader {
 public:
  FrameHeader() {}

  FrameHeader(FrameType ty, FrameFlags fflags, StreamId stream)
      : type{ty}, flags{fflags}, streamId{stream} {}

  bool flagsComplete() const {
    return !!(flags & FrameFlags::COMPLETE);
  }

  bool flagsNext() const {
    return !!(flags & FrameFlags::NEXT);
  }

  FrameType type{FrameType::RESERVED};
  FrameFlags flags{FrameFlags::EMPTY};
  StreamId streamId{0};
};

std::ostream& operator<<(std::ostream&, const FrameHeader&);
}
