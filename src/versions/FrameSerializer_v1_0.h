// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/FrameSerializer.h"

namespace reactivesocket {

class FrameSerializerV1_0 : public FrameSerializer {
 public:
  std::string protocolVersion() override;
};
} // reactivesocket
