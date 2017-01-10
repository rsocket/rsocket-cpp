// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>
#include <memory>
#include <string>
#include "src/Common.h"

namespace folly {

namespace io {
class Cursor;
class QueueAppender;
} // io
} // folly

namespace reactivesocket {

// TODO(lehecka): share the definition with Frame.h
using FrameFlags = uint16_t;
const FrameFlags FrameFlags_METADATA = 0x4000;

/// The type of a read-only view on a binary buffer.
/// MUST manage the lifetime of the underlying buffer.
struct Payload {
  Payload() = default;
  explicit Payload(
      std::unique_ptr<folly::IOBuf> _data,
      std::unique_ptr<folly::IOBuf> _metadata =
          std::unique_ptr<folly::IOBuf>());
  explicit Payload(
      const std::string& data,
      const std::string& metadata = std::string());

  explicit operator bool() const {
    return data != nullptr || metadata != nullptr;
  }

  FrameFlags getFlags() const {
    return (metadata != nullptr ? FrameFlags_METADATA : 0);
  }

  void checkFlags(FrameFlags flags) const;

  uint32_t framingSize() const {
    return (metadata != nullptr ? sizeof(uint32_t) : 0);
  }

  void serializeInto(folly::io::QueueAppender& appender);
  void deserializeFrom(folly::io::Cursor& cur, FrameFlags flags);

  static void serializeMetadataInto(
      folly::io::QueueAppender& appender,
      std::unique_ptr<folly::IOBuf> metadata);
  static std::unique_ptr<folly::IOBuf> deserializeMetadataFrom(
      folly::io::Cursor& cur,
      FrameFlags flags);
  static std::unique_ptr<folly::IOBuf> deserializeDataFrom(
      folly::io::Cursor& cur);

  std::string moveDataToString();

  void clear();

  std::unique_ptr<folly::IOBuf> data;
  std::unique_ptr<folly::IOBuf> metadata;
};

std::ostream& operator<<(std::ostream& os, const Payload& payload);
} // reactivesocket
