// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include <limits>

/// Needed for inline d'tors of frames.
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>

#include "reactivesocket-cpp/src/Payload.h"

namespace folly {
template <typename V>
class Optional;
namespace io {
class Cursor;
class QueueAppender;
}
}

namespace reactivesocket {

/// A unique identifier of a stream.
// TODO(stupaq): string typedef and forward declarations all around
using StreamId = uint32_t;

enum class FrameType : uint16_t {
  // TODO(stupaq): commented frame types indicate unimplemented frames
  RESERVED = 0x0000,
  SETUP = 0x0001,
  // LEASE = 0x0002,
  KEEPALIVE = 0x0003,
  // REQUEST_RESPONSE = 0x0004,
  // REQUEST_FNF = 0x0005,
  // REQUEST_STREAM = 0x0006,
  REQUEST_SUB = 0x0007,
  REQUEST_CHANNEL = 0x0008,
  REQUEST_N = 0x0009,
  CANCEL = 0x000A,
  RESPONSE = 0x000B,
  ERROR = 0x000C,
  // METADATA_PUSH = 0x000D,
  // EXT = 0xFFFF,
};
std::ostream& operator<<(std::ostream&, FrameType);

enum class ErrorCode : uint32_t {
  RESERVED = 0x00000000,
  // INVALID_SETUP = 0x00000001,
  // UNSUPPORTED_SETUP = 0x00000002,
  // REJECTED_SETUP = 0x00000003,
  // CONNECTION_ERROR = 0x00000101,
  APPLICATION_ERROR = 0x00000201,
  REJECTED = 0x00000202,
  CANCELED = 0x00000203,
  INVALID = 0x00000204,
  // EXT = 0xFFFFFFFF,
};
std::ostream& operator<<(std::ostream&, FrameType);

// TODO(stupaq): strong typedef
using FrameFlags = uint16_t;
const FrameFlags FrameFlags_EMPTY = 0x0000;
// const FrameFlags FrameFlags_IGNORE = 0x8000;
const FrameFlags FrameFlags_METADATA = 0x4000;
// const FrameFlags FrameFlags_FOLLOWS = 0x2000;
const FrameFlags FrameFlags_KEEPALIVE_RESPOND = 0x2000;
// const FrameFlags FrameFlags_LEASE = 0x2000;
const FrameFlags FrameFlags_COMPLETE = 0x1000;
// const FrameFlags FrameFlags_STRICT = 0x1000;
const FrameFlags FrameFlags_REQN_PRESENT = 0x0800;

class FrameHeader {
 public:
  static constexpr size_t kSize = 8;

  static FrameType peekType(const folly::IOBuf& in);

  static folly::Optional<StreamId> peekStreamId(const folly::IOBuf& in);

  FrameHeader() {
#ifndef NDEBUG
    type_ = FrameType::RESERVED;
#endif // NDEBUG
  }
  FrameHeader(FrameType type, FrameFlags flags, StreamId streamId)
      : type_(type), flags_(flags), streamId_(streamId) {}

  void serializeInto(folly::io::QueueAppender& appender);
  bool deserializeFrom(folly::io::Cursor& cur);

  FrameType type_;
  FrameFlags flags_;
  StreamId streamId_;
};
std::ostream& operator<<(std::ostream&, const FrameHeader&);

class FrameBufferAllocator {
 public:
  static std::unique_ptr<folly::IOBuf> allocate(size_t size);

  virtual ~FrameBufferAllocator() = default;

 private:
  virtual std::unique_ptr<folly::IOBuf> allocateBuffer(size_t size);
};

class FrameMetadata {
 public:
  explicit FrameMetadata(std::unique_ptr<folly::IOBuf> metadataPayload)
  : metadataPayload_(std::move(metadataPayload)) {}
  /// Empty metadata
  explicit FrameMetadata()
  : metadataPayload_(nullptr) {}

  static FrameMetadata empty();
  void checkFlags(FrameFlags flags);

  void serializeInto(folly::io::QueueAppender& appender);

  /// if metadata is present, deserializes it into metadata
  static bool deserializeFrom(folly::io::Cursor& cur,
                              const FrameFlags& flags,
                              FrameMetadata& metadata);
  bool deserializeFrom(folly::io::Cursor& cur);

  std::unique_ptr<folly::IOBuf> metadataPayload_;
};
std::ostream& operator<<(std::ostream&, const FrameMetadata&);


/// @{
/// Frames do not form hierarchy, as we never perform type erasure on a frame.
///
/// Since frames are only meaningful for stream automata on both ends of a
/// stream, intermediate layers that are frame-type-agnostic pass around
/// serialized frame.
class Frame_REQUEST_SUB {
 public:
  static constexpr bool Trait_CarriesAllowance = true;

  Frame_REQUEST_SUB() {}
  Frame_REQUEST_SUB(
      StreamId streamId,
      FrameFlags flags,
      uint32_t requestN,
      FrameMetadata metadata,
      Payload data)
      : header_(FrameType::REQUEST_SUB, flags, streamId),
        requestN_(requestN),
        metadata_(std::move(metadata)),
        data_(std::move(data)) {
    metadata_.checkFlags(flags);
  }

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_SUB(StreamId streamId, FrameFlags flags, FrameMetadata metadata, Payload data)
      : Frame_REQUEST_SUB(streamId, flags, 0, std::move(metadata), std::move(data)) {}

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  uint32_t requestN_;
  FrameMetadata metadata_;
  Payload data_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_SUB&);

class Frame_REQUEST_CHANNEL {
 public:
  static constexpr bool Trait_CarriesAllowance = true;

  Frame_REQUEST_CHANNEL() {}
  Frame_REQUEST_CHANNEL(
      StreamId streamId,
      FrameFlags flags,
      uint32_t requestN,
      FrameMetadata metadata,
      Payload data)
      : header_(FrameType::REQUEST_CHANNEL, flags, streamId),
        requestN_(requestN),
        metadata_(std::move(metadata)),
        data_(std::move(data)) {
    metadata_.checkFlags(flags);
  }

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_CHANNEL(StreamId streamId, FrameFlags flags, FrameMetadata metadata, Payload data)
      : Frame_REQUEST_CHANNEL(streamId, flags, 0, std::move(metadata), std::move(data)) {}

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  uint32_t requestN_;
  FrameMetadata metadata_;
  Payload data_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_CHANNEL&);

class Frame_REQUEST_N {
 public:
  static constexpr bool Trait_CarriesAllowance = true;
  static constexpr size_t kMaxRequestN = std::numeric_limits<uint32_t>::max();

  Frame_REQUEST_N() {}
  Frame_REQUEST_N(StreamId streamId, uint32_t requestN)
      : header_(FrameType::REQUEST_N, FrameFlags_EMPTY, streamId),
        requestN_(requestN) {}

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  uint32_t requestN_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_N&);

class Frame_CANCEL {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_CANCEL() {}
  explicit Frame_CANCEL(StreamId streamId)
      : Frame_CANCEL(streamId, FrameFlags_EMPTY, FrameMetadata::empty()) {}
  Frame_CANCEL(StreamId streamId, FrameFlags flags, FrameMetadata metadata)
      : header_(FrameType::CANCEL, flags, streamId), metadata_(std::move(metadata)) {
    metadata_.checkFlags(flags);
  }

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  FrameMetadata metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_CANCEL&);

class Frame_RESPONSE {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_RESPONSE() {}
  Frame_RESPONSE(StreamId streamId, FrameFlags flags, FrameMetadata metadata, Payload data)
      : header_(FrameType::RESPONSE, flags, streamId), metadata_(std::move(metadata)), data_(std::move(data)) {
    metadata_.checkFlags(flags);
  }

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  FrameMetadata metadata_;
  Payload data_;
};
std::ostream& operator<<(std::ostream&, const Frame_RESPONSE&);

class Frame_ERROR {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_ERROR() {}
  Frame_ERROR(StreamId streamId, ErrorCode errorCode)
      : Frame_ERROR(streamId, FrameFlags_EMPTY, errorCode, FrameMetadata::empty()) {}
  Frame_ERROR(StreamId streamId, FrameFlags flags, ErrorCode errorCode, FrameMetadata metadata)
      : header_(FrameType::ERROR, flags, streamId),
        errorCode_(errorCode),
        metadata_(std::move(metadata)) {
    metadata_.checkFlags(flags);
  }

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  ErrorCode errorCode_;
  FrameMetadata metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_ERROR&);

class Frame_KEEPALIVE {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_KEEPALIVE() {}
  Frame_KEEPALIVE(StreamId streamId, FrameFlags flags, Payload data)
      : header_(FrameType::KEEPALIVE, flags, streamId),
        data_(std::move(data)) {}

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  Payload data_;
};
std::ostream& operator<<(std::ostream&, const Frame_KEEPALIVE&);

class Frame_SETUP {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_SETUP() {}
  Frame_SETUP(
      StreamId streamId,
      FrameFlags flags,
      uint32_t version,
      uint32_t keepaliveTime,
      uint32_t maxLifetime,
      FrameMetadata metadata,
      Payload data)
      : header_(FrameType::SETUP, flags, streamId),
        version_(version),
        keepaliveTime_(keepaliveTime),
        maxLifetime_(maxLifetime),
        metadata_(std::move(metadata)),
        data_(std::move(data)) {
    metadata_.checkFlags(flags);
  }

  Payload serializeOut();
  bool deserializeFrom(Payload in);

  FrameHeader header_;
  uint32_t version_;
  uint32_t keepaliveTime_;
  uint32_t maxLifetime_;
  FrameMetadata metadata_;
  Payload data_;
};
std::ostream& operator<<(std::ostream&, const Frame_SETUP&);
/// @}
}
