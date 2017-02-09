// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <string>

namespace reactivesocket {

// interface separating serialization/deserialization of ReactiveSocket frames
class FrameSerializer {
 public:
  virtual ~FrameSerializer() = default;

  virtual std::string protocolVersion() = 0;

  // TODO: serialize/deserialize methods

  static std::unique_ptr<FrameSerializer> createFrameSerializer(
      std::string protocolVersion);
};
} // reactivesocket
