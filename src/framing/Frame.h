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

namespace rsocket {

/// A unique identifier of a stream.
// TODO(stupaq): strong typedef and forward declarations all around
using StreamId = uint32_t;

enum class FrameType : uint8_t {
  RESERVED = 0x00,
  SETUP = 0x01,
  LEASE = 0x02,
  KEEPALIVE = 0x03,
  REQUEST_RESPONSE = 0x04,
  REQUEST_FNF = 0x05,
  REQUEST_STREAM = 0x06,
  REQUEST_CHANNEL = 0x07,
  REQUEST_N = 0x08,
  CANCEL = 0x09,
  PAYLOAD = 0x0A,
  ERROR = 0x0B,
  METADATA_PUSH = 0x0C,
  RESUME = 0x0D,
  RESUME_OK = 0x0E,
  EXT = 0x3F,
};
std::string to_string(FrameType);
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
  // The server rejected the resume, it can specify the reason in the payload.
  // Stream ID MUST be 0.
  REJECTED_RESUME = 0x00000004,
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

enum class FrameFlags : uint16_t {
  EMPTY = 0x000,
  IGNORE = 0x200,
  METADATA = 0x100,

  // SETUP frame
  RESUME_ENABLE = 0x80,
  LEASE = 0x40,

  // KEEPALIVE frame
  KEEPALIVE_RESPOND = 0x80,

  // REQUEST_RESPONSE, REQUEST_FNF, REQUEST_STREAM, REQUEST_CHANNEL,
  // PAYLOAD frame
  FOLLOWS = 0x80,

  // REQUEST_CHANNEL, PAYLOAD frame
  COMPLETE = 0x40,

  // PAYLOAD frame
  NEXT = 0x20,
};

std::ostream& operator<<(std::ostream&, FrameFlags);

constexpr inline FrameFlags operator|(FrameFlags a, FrameFlags b) {
  return static_cast<FrameFlags>(
      static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

constexpr inline FrameFlags operator&(FrameFlags a, FrameFlags b) {
  return static_cast<FrameFlags>(
      static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline FrameFlags& operator|=(FrameFlags& a, FrameFlags b) {
  return a = (a | b);
}

inline FrameFlags& operator&=(FrameFlags& a, FrameFlags b) {
  return a = (a & b);
}

constexpr inline bool operator!(FrameFlags a) {
  return !static_cast<uint16_t>(a);
}

constexpr inline FrameFlags operator~(FrameFlags a) {
  return static_cast<FrameFlags>(~static_cast<uint16_t>(a));
}

class FrameHeader {
 public:
  FrameHeader() {
#ifndef NDEBUG
    type_ = FrameType::RESERVED;
#endif // NDEBUG
  }
  FrameHeader(FrameType type, FrameFlags flags, StreamId streamId)
      : type_(type), flags_(flags), streamId_(streamId) {}

  bool flagsComplete() const {
    return !!(flags_ & FrameFlags::COMPLETE);
  }

  bool flagsNext() const {
    return !!(flags_ & FrameFlags::NEXT);
  }

  FrameType type_{};
  FrameFlags flags_{};
  StreamId streamId_{};
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

class Frame_REQUEST_N {
 public:
  /*
   * Maximum value for ReactiveSocket Subscription::request.
   * Value is a signed int, however negative values are not allowed.
   *
   * n.b. this is less than size_t because of the Frame encoding restrictions.
   */
  static constexpr int64_t kMaxRequestN = std::numeric_limits<int32_t>::max();

  Frame_REQUEST_N() = default;
  Frame_REQUEST_N(StreamId streamId, uint32_t requestN)
      : header_(FrameType::REQUEST_N, FrameFlags::EMPTY, streamId),
        requestN_(requestN) {
    DCHECK(requestN_ > 0);
    DCHECK(requestN_ <= kMaxRequestN);
  }

  FrameHeader header_;
  uint32_t requestN_{};
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_N&);

class Frame_REQUEST_Base {
 public:
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
    // to verify the client didn't set
    // METADATA and provided none
    payload_.checkFlags(header_.flags_);
    // TODO: DCHECK(requestN_ > 0);
    DCHECK(requestN_ <= Frame_REQUEST_N::kMaxRequestN);
  }

  /// For compatibility with other data-carrying frames.
  Frame_REQUEST_Base(
      FrameType frameType,
      StreamId streamId,
      FrameFlags flags,
      Payload payload)
      : Frame_REQUEST_Base(frameType, streamId, flags, 0, std::move(payload)) {}

  FrameHeader header_;
  uint32_t requestN_{};
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_Base&);

class Frame_REQUEST_STREAM : public Frame_REQUEST_Base {
 public:
  constexpr static const FrameFlags AllowedFlags =
      FrameFlags::METADATA | FrameFlags::FOLLOWS;

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
      : Frame_REQUEST_STREAM(
            streamId,
            flags & AllowedFlags,
            0,
            std::move(payload)) {}
};
std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_STREAM& frame);

class Frame_REQUEST_CHANNEL : public Frame_REQUEST_Base {
 public:
  constexpr static const FrameFlags AllowedFlags =
      FrameFlags::METADATA | FrameFlags::FOLLOWS | FrameFlags::COMPLETE;

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
      : Frame_REQUEST_CHANNEL(
            streamId,
            flags & AllowedFlags,
            0,
            std::move(payload)) {}
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_CHANNEL&);

class Frame_REQUEST_RESPONSE {
 public:
  constexpr static const FrameFlags AllowedFlags =
      FrameFlags::METADATA | FrameFlags::FOLLOWS;

  Frame_REQUEST_RESPONSE() = default;
  Frame_REQUEST_RESPONSE(StreamId streamId, FrameFlags flags, Payload payload)
      : header_(
            FrameType::REQUEST_RESPONSE,
            (flags & AllowedFlags) | payload.getFlags(),
            streamId),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  FrameHeader header_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_RESPONSE&);

class Frame_REQUEST_FNF {
 public:
  constexpr static const FrameFlags AllowedFlags =
      FrameFlags::METADATA | FrameFlags::FOLLOWS;

  Frame_REQUEST_FNF() = default;
  Frame_REQUEST_FNF(StreamId streamId, FrameFlags flags, Payload payload)
      : header_(
            FrameType::REQUEST_FNF,
            (flags & AllowedFlags) | payload.getFlags(),
            streamId),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  FrameHeader header_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_REQUEST_FNF&);

class Frame_METADATA_PUSH {
 public:
  Frame_METADATA_PUSH() {}
  explicit Frame_METADATA_PUSH(std::unique_ptr<folly::IOBuf> metadata)
      : header_(FrameType::METADATA_PUSH, FrameFlags::METADATA, 0),
        metadata_(std::move(metadata)) {
    CHECK(metadata_);
  }

  FrameHeader header_;
  std::unique_ptr<folly::IOBuf> metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_METADATA_PUSH&);

class Frame_CANCEL {
 public:
  Frame_CANCEL() = default;
  explicit Frame_CANCEL(StreamId streamId)
      : header_(FrameType::CANCEL, FrameFlags::EMPTY, streamId) {}

  FrameHeader header_;
};
std::ostream& operator<<(std::ostream&, const Frame_CANCEL&);

class Frame_PAYLOAD {
 public:
  constexpr static const FrameFlags AllowedFlags = FrameFlags::METADATA |
      FrameFlags::FOLLOWS | FrameFlags::COMPLETE | FrameFlags::NEXT;

  Frame_PAYLOAD() = default;
  Frame_PAYLOAD(StreamId streamId, FrameFlags flags, Payload payload)
      : header_(
            FrameType::PAYLOAD,
            (flags & AllowedFlags) | payload.getFlags(),
            streamId),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
  }

  static Frame_PAYLOAD complete(StreamId streamId);

  FrameHeader header_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_PAYLOAD&);

class Frame_ERROR {
 public:
  constexpr static const FrameFlags AllowedFlags = FrameFlags::METADATA;

  Frame_ERROR() = default;
  Frame_ERROR(StreamId streamId, ErrorCode errorCode, Payload payload)
      : header_(FrameType::ERROR, payload.getFlags(), streamId),
        errorCode_(errorCode),
        payload_(std::move(payload)) {}

  static Frame_ERROR unexpectedFrame();
  static Frame_ERROR invalidFrame();
  static Frame_ERROR badSetupFrame(const std::string& message);
  static Frame_ERROR connectionError(const std::string& message);
  static Frame_ERROR rejectedResume(const std::string& message);
  static Frame_ERROR error(StreamId streamId, Payload&& payload);
  static Frame_ERROR applicationError(StreamId streamId, Payload&& payload);

  FrameHeader header_;
  ErrorCode errorCode_{};
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_ERROR&);

class Frame_KEEPALIVE {
 public:
  constexpr static const FrameFlags AllowedFlags =
      FrameFlags::KEEPALIVE_RESPOND;

  Frame_KEEPALIVE() = default;
  Frame_KEEPALIVE(
      FrameFlags flags,
      ResumePosition position,
      std::unique_ptr<folly::IOBuf> data)
      : header_(FrameType::KEEPALIVE, flags & AllowedFlags, 0),
        position_(position),
        data_(std::move(data)) {}

  FrameHeader header_;
  ResumePosition position_{};
  std::unique_ptr<folly::IOBuf> data_;
};
std::ostream& operator<<(std::ostream&, const Frame_KEEPALIVE&);

class SetupParameters;

class Frame_SETUP {
 public:
  constexpr static const FrameFlags AllowedFlags =
      FrameFlags::METADATA | FrameFlags::RESUME_ENABLE | FrameFlags::LEASE;

  constexpr static const uint32_t kMaxKeepaliveTime =
      std::numeric_limits<int32_t>::max();
  constexpr static const uint32_t kMaxLifetime =
      std::numeric_limits<int32_t>::max();

  Frame_SETUP() = default;
  Frame_SETUP(
      FrameFlags flags,
      uint16_t versionMajor,
      uint16_t versionMinor,
      uint32_t keepaliveTime,
      uint32_t maxLifetime,
      const ResumeIdentificationToken& token,
      std::string metadataMimeType,
      std::string dataMimeType,
      Payload payload)
      : header_(
            FrameType::SETUP,
            (flags & AllowedFlags) | payload.getFlags(),
            0),
        versionMajor_(versionMajor),
        versionMinor_(versionMinor),
        keepaliveTime_(keepaliveTime),
        maxLifetime_(maxLifetime),
        token_(token),
        metadataMimeType_(metadataMimeType),
        dataMimeType_(dataMimeType),
        payload_(std::move(payload)) {
    payload_.checkFlags(header_.flags_); // to verify the client didn't set
    // METADATA and provided none
    DCHECK(keepaliveTime_ > 0);
    DCHECK(maxLifetime_ > 0);
    DCHECK(keepaliveTime_ <= kMaxKeepaliveTime);
    DCHECK(maxLifetime_ <= kMaxLifetime);
  }

  void moveToSetupPayload(SetupParameters& setupPayload);

  FrameHeader header_;
  uint16_t versionMajor_{};
  uint16_t versionMinor_{};
  uint32_t keepaliveTime_{};
  uint32_t maxLifetime_{};
  ResumeIdentificationToken token_;
  std::string metadataMimeType_;
  std::string dataMimeType_;
  Payload payload_;
};
std::ostream& operator<<(std::ostream&, const Frame_SETUP&);
/// @}

class Frame_LEASE {
 public:
  constexpr static const FrameFlags AllowedFlags = FrameFlags::METADATA;
  constexpr static const uint32_t kMaxTtl = std::numeric_limits<int32_t>::max();
  constexpr static const uint32_t kMaxNumRequests =
      std::numeric_limits<int32_t>::max();

  Frame_LEASE() = default;
  Frame_LEASE(
      uint32_t ttl,
      uint32_t numberOfRequests,
      std::unique_ptr<folly::IOBuf> metadata = std::unique_ptr<folly::IOBuf>())
      : header_(
            FrameType::LEASE,
            metadata ? FrameFlags::METADATA : FrameFlags::EMPTY,
            0),
        ttl_(ttl),
        numberOfRequests_(numberOfRequests),
        metadata_(std::move(metadata)) {
    DCHECK(ttl_ > 0);
    DCHECK(numberOfRequests_ > 0);
    DCHECK(ttl_ <= kMaxTtl);
    DCHECK(numberOfRequests_ <= kMaxNumRequests);
  }

  FrameHeader header_;
  uint32_t ttl_{};
  uint32_t numberOfRequests_{};
  std::unique_ptr<folly::IOBuf> metadata_;
};
std::ostream& operator<<(std::ostream&, const Frame_LEASE&);
/// @}

class Frame_RESUME {
 public:
  Frame_RESUME() = default;
  Frame_RESUME(
      const ResumeIdentificationToken& token,
      ResumePosition lastReceivedServerPosition,
      ResumePosition clientPosition,
      ProtocolVersion protocolVersion)
      : header_(FrameType::RESUME, FrameFlags::EMPTY, 0),
        versionMajor_(protocolVersion.major),
        versionMinor_(protocolVersion.minor),
        token_(token),
        lastReceivedServerPosition_(lastReceivedServerPosition),
        clientPosition_(clientPosition) {}

  FrameHeader header_;
  uint16_t versionMajor_{};
  uint16_t versionMinor_{};
  ResumeIdentificationToken token_;
  ResumePosition lastReceivedServerPosition_{};
  ResumePosition clientPosition_{};
};
std::ostream& operator<<(std::ostream&, const Frame_RESUME&);
/// @}

class Frame_RESUME_OK {
 public:
  Frame_RESUME_OK() = default;
  explicit Frame_RESUME_OK(ResumePosition position)
      : header_(FrameType::RESUME_OK, FrameFlags::EMPTY, 0),
        position_(position) {}

  FrameHeader header_;
  ResumePosition position_{};
};
std::ostream& operator<<(std::ostream&, const Frame_RESUME_OK&);
/// @}
}
