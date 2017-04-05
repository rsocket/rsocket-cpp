// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/versions/FrameSerializer_v0.h"

namespace reactivesocket {

class FrameSerializerV0_1 : public FrameSerializerV0 {
 public:
  constexpr static const ProtocolVersion Version = ProtocolVersion(0, 1);

  static ProtocolVersion detectProtocolVersion(const folly::IOBuf& firstFrame);

  ProtocolVersion protocolVersion() override;
};
} // reactivesocket
