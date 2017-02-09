// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/versions/FrameSerializer_v0.h"

namespace reactivesocket {

class FrameSerializerV0_1 : public FrameSerializerV0 {
 public:
  std::string protocolVersion() override {
    return "0.1";
  }
};
} // reactivesocket
