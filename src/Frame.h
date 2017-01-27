// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <array>
#include <iosfwd>
#include <limits>

/// Needed for inline d'tors of frames.
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>

#include "src/Payload.h"

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
// TODO(stupaq): strong typedef and forward declarations all around
using StreamId = uint32_t;

enum class FrameType : uint16_t {
  // TODO(stupaq): commented frame types indicate unimplemented frames
  RESERVED = 0x0000,
  SETUP = 0x0001,
  LEASE = 0x0002,
  KEEPALIVE = 0x0003,
  REQUEST_RESPONSE = 0x0004,
  REQUEST_FNF = 0x0005,
  REQUEST_STREAM = 0x0006,
  REQUEST_SUB = 0x0007,
  REQUEST_CHANNEL = 0x0008,
  REQUEST_N = 0x0009,
  CANCEL = 0x000A,
  RESPONSE = 0x000B,
  ERROR = 0x000C,
  METADATA_PUSH = 0x000D,
  RESUME = 0x000E,
  RESUME_OK = 0x000F
  // EXT = 0xFFFF,
};
std::ostream& operator<<(std::ostream&, FrameType);

enum class ErrorCode : uint32_t {
  RESERVED = 0x00000000,
  // The Setup frame is invalid for the server (it could be that the client is
  // too recent for the old server). Stream ID MUST be 0.
  INVALID_SETUP = 0x00000001,
  // Some (or all) of the parameters specified by the client are unsupported by
  // the server. Stream ID MUST be 0.
  UNSUPPORTED_SETUP = 0x00000002,
  // The server rejected the setup, it can specify the reason in the payload.
  // Stream ID MUST be 0.
  REJECTED_SETUP = 0x00000003,
  // The connection is being terminated. Stream ID MUST be 0.
  CONNECTION_ERROR = 0x00000101,
  // Application layer logic generating a Reactive Streams onError event.
  // Stream ID MUST be non-0.
  APPLICATION_ERROR = 0x00000201,
  // Despite being a valid request, the Responder decided to reject it. The
  // Responder guarantees that it didn't process the request. The reason for the
  // rejection is explained in the metadata section. Stream ID MUST be non-0.
  REJECTED = 0x00000202,
  // The responder canceled the request but potentially have started processing
  // it (almost identical to REJECTED but doesn't garantee that no side-effect
  // have been started). Stream ID MUST be non-0.
  CANCELED = 0x00000203,
  // The request is invalid. Stream ID MUST be non-0.
  INVALID = 0x00000204,
  // EXT = 0xFFFFFFFF,
};
std::ostream& operator<<(std::ostream&, ErrorCode);

// TODO(stupaq): strong typedef
using FrameFlags = uint16_t;
const FrameFlags FrameFlags_EMPTY = 0x0000;
// const FrameFlags FrameFlags_IGNORE = 0x8000;
// const FrameFlags FrameFlags_METADATA = 0x4000;
// const FrameFlags FrameFlags_FOLLOWS = 0x2000;
const FrameFlags FrameFlags_KEEPALIVE_RESPOND = 0x2000;
const FrameFlags FrameFlags_LEASE = 0x2000;
const FrameFlags FrameFlags_COMPLETE = 0x1000;
// const FrameFlags FrameFlags_STRICT = 0x1000;
const FrameFlags FrameFlags_REQN_PRESENT = 0x0800;
const FrameFlags FrameFlags_RESUME_ENABLE = 0x0800;

class FrameHeader {
 public:
  static constexpr size_t kSize = 8;

  static FrameType peekType(const folly::IOBuf& p);
  static folly::Optional<StreamId> peekStreamId(const folly::IOBuf& p);

  FrameHeader() {
#ifndef NDEBUG
    type_ = FrameType::RESERVED;
#endif // NDEBUG
  }
  FrameHeader(FrameType type, FrameFlags flags, StreamId streamId)
      : type_(type), flags_(flags), streamId_(streamId) {}

  void serializeInto(folly::io::QueueAppender& appender);
  void deserializeFrom(folly::io::Cursor& cur);

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

/// @{
/// Frames do not form hierarchy, as we never perform type erasure on a frame.
/// We use inheritance only to save code duplication.
///
/// Since frames are only meaningful for stream automata on both ends of a
/// stream, intermediate layers that are frame-type-agnostic pass around
/// serialized frame.

class Frame_REQUEST_Base {
 public:
  static constexpr bool Trait_CarriesAllowance = true;

  Frame_REQUEST_Base() = default;
  Frame_REQUEST_Base(
      FrameType frameType,
      StreamId streamId,
      FrameFlags flags,
      uint32_t requestN,
      Payload payload)
      : header_(frameType, flags | payload.getFlags(), streamId),
        requestN_(requestN),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_Base(
      FrameType frameType,
      StreamId streamId,
      FrameFlags flags,
      Payload payload)
      : Frame_REQUEST_Base(frameType, streamId, flags, 0, std::move(payload)) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  uint32_t requestN_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_Base&);

class Frame_REQUEST_STREAM : public Frame_REQUEST_Base {
 public:
  Frame_REQUEST_STREAM() = default;
  Frame_REQUEST_STREAM(
      StreamId streamId,
      FrameFlags flags,
      uint32_t requestN,
      Payload payload)
      : Frame_REQUEST_Base(
            FrameType::REQUEST_STREAM,
            streamId,
            flags,
            requestN,
            std::move(payload)) {}

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_STREAM(StreamId streamId, FrameFlags flags, Payload payload)
      : Frame_REQUEST_STREAM(streamId, flags, 0, std::move(payload)) {}
};

class Frame_REQUEST_SUB : public Frame_REQUEST_Base {
 public:
  Frame_REQUEST_SUB() = default;
  Frame_REQUEST_SUB(
      StreamId streamId,
      FrameFlags flags,
      uint32_t requestN,
      Payload payload)
      : Frame_REQUEST_Base(
            FrameType::REQUEST_SUB,
            streamId,
            flags,
            requestN,
            std::move(payload)) {}

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_SUB(StreamId streamId, FrameFlags flags, Payload payload)
      : Frame_REQUEST_SUB(streamId, flags, 0, std::move(payload)) {}
};

class Frame_REQUEST_CHANNEL : public Frame_REQUEST_Base {
 public:
  Frame_REQUEST_CHANNEL() = default;
  Frame_REQUEST_CHANNEL(
      StreamId streamId,
      FrameFlags flags,
      uint32_t requestN,
      Payload payload)
      : Frame_REQUEST_Base(
            FrameType::REQUEST_CHANNEL,
            streamId,
            flags,
            requestN,
            std::move(payload)) {}

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_CHANNEL(StreamId streamId, FrameFlags flags, Payload payload)
      : Frame_REQUEST_CHANNEL(streamId, flags, 0, std::move(payload)) {}
};

class Frame_REQUEST_RESPONSE {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_REQUEST_RESPONSE() = default;
  Frame_REQUEST_RESPONSE(StreamId streamId, FrameFlags flags, Payload payload)
      : header_(
            FrameType::REQUEST_RESPONSE,
            flags | payload.getFlags(),
            streamId),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_RESPONSE&);

class Frame_REQUEST_FNF {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_REQUEST_FNF() = default;
  Frame_REQUEST_FNF(StreamId streamId, FrameFlags flags, Payload payload)
      : header_(FrameType::REQUEST_FNF, flags | payload.getFlags(), streamId),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_FNF&);

class Frame_REQUEST_N {
 public:
  static constexpr bool Trait_CarriesAllowance = true;

  /*
   * Maximum value for ReactiveSocket Subscription::request.
   * Value is a signed int, however negative values are not allowed.
   *
   * n.b. this is less than size_t because of the Frame encoding restrictions.
   */
  static constexpr size_t kMaxRequestN = std::numeric_limits<int32_t>::max();

  Frame_REQUEST_N() = default;
  Frame_REQUEST_N(StreamId streamId, uint32_t requestN)
      : header_(FrameType::REQUEST_N, FrameFlags_EMPTY, streamId),
        requestN_(requestN) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  uint32_t requestN_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_N&);

class Frame_METADATA_PUSH {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_METADATA_PUSH() {}
  explicit Frame_METADATA_PUSH(std::unique_ptr<folly::IOBuf> metadata)
      : header_(FrameType::METADATA_PUSH, FrameFlags_METADATA, 0),
        metadata_(std::move(metadata)) {
    CHECK(metadata_);
  }

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  std::unique_ptr<folly::IOBuf> metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_METADATA_PUSH&);

class Frame_CANCEL {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_CANCEL() = default;
  explicit Frame_CANCEL(
      StreamId streamId,
      std::unique_ptr<folly::IOBuf> metadata = std::unique_ptr<folly::IOBuf>())
      : header_(
            FrameType::CANCEL,
            metadata ? FrameFlags_METADATA : 0,
            streamId),
        metadata_(std::move(metadata)) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  std::unique_ptr<folly::IOBuf> metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_CANCEL&);

class Frame_RESPONSE {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_RESPONSE() = default;
  Frame_RESPONSE(StreamId streamId, FrameFlags flags, Payload payload)
      : header_(FrameType::RESPONSE, flags | payload.getFlags(), streamId),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  static Frame_RESPONSE complete(StreamId streamId);

  FrameHeader header_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_RESPONSE&);

class Frame_ERROR {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_ERROR() = default;
  Frame_ERROR(StreamId streamId, ErrorCode errorCode, Payload payload)
      : header_(FrameType::ERROR, payload.getFlags(), streamId),
        errorCode_(errorCode),
        payload_(std::move(payload)) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  static Frame_ERROR unexpectedFrame();
  static Frame_ERROR badSetupFrame(const std::string& message);
  static Frame_ERROR connectionError(const std::string& message);
  static Frame_ERROR invalid(StreamId streamId, const std::string& message);
  static Frame_ERROR applicationError(
      StreamId streamId,
      const std::string& message);

  FrameHeader header_;
  ErrorCode errorCode_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_ERROR&);

class Frame_KEEPALIVE {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_KEEPALIVE() = default;
  Frame_KEEPALIVE(
      FrameFlags flags,
      ResumePosition position,
      std::unique_ptr<folly::IOBuf> data)
      : header_(FrameType::KEEPALIVE, flags, 0),
        position_(position),
        data_(std::move(data)) {
    assert(!(flags & FrameFlags_METADATA));
  }

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  ResumePosition position_;
  std::unique_ptr<folly::IOBuf> data_;
};
std::ostream& operator<<(std::ostream&, const Frame_KEEPALIVE&);

class Frame_SETUP {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_SETUP() = default;
  Frame_SETUP(
      FrameFlags flags,
      uint32_t version,
      uint32_t keepaliveTime,
      uint32_t maxLifetime,
      const ResumeIdentificationToken& token,
      std::string metadataMimeType,
      std::string dataMimeType,
      Payload payload)
      : header_(FrameType::SETUP, flags | payload.getFlags(), 0),
        version_(version),
        keepaliveTime_(keepaliveTime),
        maxLifetime_(maxLifetime),
        token_(token),
        metadataMimeType_(metadataMimeType),
        dataMimeType_(dataMimeType),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  uint32_t version_;
  uint32_t keepaliveTime_;
  uint32_t maxLifetime_;
  ResumeIdentificationToken token_;
  std::string metadataMimeType_;
  std::string dataMimeType_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_SETUP&);
/// @}

class Frame_LEASE {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_LEASE() = default;
  Frame_LEASE(
      uint32_t ttl,
      uint32_t numberOfRequests,
      std::unique_ptr<folly::IOBuf> metadata = std::unique_ptr<folly::IOBuf>())
      : header_(FrameType::LEASE, metadata ? FrameFlags_METADATA : 0, 0),
        ttl_(ttl),
        numberOfRequests_(numberOfRequests),
        metadata_(std::move(metadata)) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  uint32_t ttl_;
  uint32_t numberOfRequests_;
  std::unique_ptr<folly::IOBuf> metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_LEASE&);
/// @}

class Frame_RESUME {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_RESUME() = default;
  Frame_RESUME(const ResumeIdentificationToken& token, ResumePosition position)
      : header_(FrameType::RESUME, 0, 0), token_(token), position_(position) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  ResumeIdentificationToken token_;
  ResumePosition position_;
};
std::ostream& operator<<(std::ostream&, const Frame_RESUME&);
/// @}

class Frame_RESUME_OK {
 public:
  static constexpr bool Trait_CarriesAllowance = false;

  Frame_RESUME_OK() = default;
  explicit Frame_RESUME_OK(ResumePosition position)
      : header_(FrameType::RESUME_OK, 0, 0), position_(position) {}

  std::unique_ptr<folly::IOBuf> serializeOut();
  bool deserializeFrom(std::unique_ptr<folly::IOBuf> in);

  FrameHeader header_;
  ResumePosition position_;
};
std::ostream& operator<<(std::ostream&, const Frame_RESUME_OK&);
/// @}
}
