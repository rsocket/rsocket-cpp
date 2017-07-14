// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <memory>
#include <string>

#include "rsocket/framing/FrameFlags.h"

namespace rsocket {

/// The type of a read-only view on a binary buffer.
/// MUST manage the lifetime of the underlying buffer.
struct Payload {
  Payload() = default;
  Payload(
      std::unique_ptr<folly::IOBuf> data,
      std::unique_ptr<folly::IOBuf> metadata = std::unique_ptr<folly::IOBuf>());
  Payload(const std::string& data, const std::string& metadata = std::string());

  explicit operator bool() const {
    return data != nullptr || metadata != nullptr;
  }

  FrameFlags getFlags() const;
  void checkFlags(FrameFlags flags) const;

  std::string moveDataToString();
  std::string cloneDataToString() const;
  void clear();

  Payload clone() const;

  std::unique_ptr<folly::IOBuf> data;
  std::unique_ptr<folly::IOBuf> metadata;
};

std::ostream& operator<<(std::ostream& os, const Payload& payload);
}
