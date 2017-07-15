// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/framing/FrameSerializer_v0.h"

namespace rsocket {

class FrameSerializerV0_1 : public FrameSerializerV0 {
 public:
  constexpr static const ProtocolVersion Version = ProtocolVersion(0, 1);
  constexpr static const size_t kMinBytesNeededForAutodetection = 12; // bytes

  static ProtocolVersion detectProtocolVersion(
      const folly::IOBuf& firstFrame,
      size_t skipBytes = 0);

  ProtocolVersion protocolVersion() override;
};
} // reactivesocket
