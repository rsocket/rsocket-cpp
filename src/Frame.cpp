// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Frame.h"

#include <bitset>

#include <folly/Memory.h>
#include <folly/Optional.h>
#include <folly/io/Cursor.h>

// TODO(stupaq): strict enum validation
// TODO(stupaq): verify whether frames contain extra data
// TODO(stupaq): get rid of these try-catch blocks
namespace reactivesocket {

static folly::IOBufQueue createBufferQueue(uint32_t bufferSize) {
  auto buf = FrameBufferAllocator::allocate(bufferSize);
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  queue.append(std::move(buf));
  return queue;
}

std::unique_ptr<folly::IOBuf> FrameBufferAllocator::allocate(size_t size) {
  // Purposely leak the allocator, since it's hard to deterministically
  // guarantee that threads will stop using it before it would get statically
  // destructed.
  static auto* singleton = new FrameBufferAllocator;
  return singleton->allocateBuffer(size);
}

std::unique_ptr<folly::IOBuf> FrameBufferAllocator::allocateBuffer(
    size_t size) {
  return folly::IOBuf::createCombined(size);
}

std::ostream& operator<<(std::ostream& os, FrameType type) {
  switch (type) {
    case FrameType::REQUEST_STREAM:
      return os << "REQUEST_STREAM";
    case FrameType::REQUEST_SUB:
      return os << "REQUEST_SUB";
    case FrameType::REQUEST_CHANNEL:
      return os << "REQUEST_CHANNEL";
    case FrameType::REQUEST_N:
      return os << "REQUEST_N";
    case FrameType::REQUEST_RESPONSE:
      return os << "REQUEST_RESPONSE";
    case FrameType::REQUEST_FNF:
      return os << "REQUEST_FNF";
    case FrameType::CANCEL:
      return os << "CANCEL";
    case FrameType::RESPONSE:
      return os << "RESPONSE";
    case FrameType::ERROR:
      return os << "ERROR";
    case FrameType::RESERVED:
      return os << "RESERVED";
    case FrameType::KEEPALIVE:
      return os << "KEEPALIVE";
    case FrameType::SETUP:
      return os << "SETUP";
    case FrameType::LEASE:
      return os << "LEASE";
    case FrameType::METADATA_PUSH:
      return os << "METADATA_PUSH";
    case FrameType::RESUME:
      return os << "RESUME";
    case FrameType::RESUME_OK:
      return os << "RESUME_OK";
  }
  return os << "FrameType(" << static_cast<uint16_t>(type) << ")";
}

std::ostream& operator<<(std::ostream& os, ErrorCode errorCode) {
  switch (errorCode) {
    case ErrorCode::RESERVED:
      return os << "RESERVED";
    case ErrorCode::APPLICATION_ERROR:
      return os << "APPLICATION_ERROR";
    case ErrorCode::REJECTED:
      return os << "REJECTED";
    case ErrorCode::CANCELED:
      return os << "CANCELED";
    case ErrorCode::INVALID:
      return os << "INVALID";
    case ErrorCode::INVALID_SETUP:
      return os << "INVALID_SETUP";
    case ErrorCode::REJECTED_SETUP:
      return os << "REJECTED_SETUP";
    case ErrorCode::UNSUPPORTED_SETUP:
      return os << "UNSUPPORTED_SETUP";
    case ErrorCode::CONNECTION_ERROR:
      return os << "CONNECTION_ERROR";
  }
  return os << "ErrorCode(" << static_cast<uint32_t>(errorCode) << ")";
}

/// @{
FrameType FrameHeader::peekType(const folly::IOBuf& in) {
  folly::io::Cursor cur(&in);
  try {
    return static_cast<FrameType>(cur.readBE<uint16_t>());
  } catch (...) {
    return FrameType::RESERVED;
  }
}

folly::Optional<StreamId> FrameHeader::peekStreamId(const folly::IOBuf& in) {
  folly::io::Cursor cur(&in);
  try {
    cur.skip(sizeof(uint16_t)); // type
    cur.skip(sizeof(uint16_t)); // flags
    return folly::make_optional(cur.readBE<uint32_t>());
  } catch (...) {
    return folly::none;
  }
}

void FrameHeader::serializeInto(folly::io::QueueAppender& appender) {
  appender.writeBE<uint16_t>(static_cast<uint16_t>(type_));
  appender.writeBE<uint16_t>(flags_);
  appender.writeBE<uint32_t>(streamId_);
}

void FrameHeader::deserializeFrom(folly::io::Cursor& cur) {
  type_ = static_cast<FrameType>(cur.readBE<uint16_t>());
  flags_ = cur.readBE<uint16_t>();
  streamId_ = cur.readBE<uint32_t>();
}

std::ostream& operator<<(std::ostream& os, const FrameHeader& header) {
  std::bitset<16> flags(header.flags_);
  return os << header.type_ << "[" << flags << ", " << header.streamId_ << "]";
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_REQUEST_Base::serializeOut() {
  auto queue = createBufferQueue(
      FrameHeader::kSize + sizeof(uint32_t) + payload_.framingSize());
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE<uint32_t>(requestN_);
  payload_.serializeInto(appender);
  return queue.move();
}

bool Frame_REQUEST_Base::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    requestN_ = cur.readBE<uint32_t>();
    payload_.deserializeFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_Base& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ", "
            << frame.payload_;
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_REQUEST_N::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + sizeof(uint32_t));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE<uint32_t>(requestN_);
  return queue.move();
}

bool Frame_REQUEST_N::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    requestN_ = cur.readBE<uint32_t>();
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_N& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ")";
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_REQUEST_RESPONSE::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + payload_.framingSize());
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  payload_.serializeInto(appender);
  return queue.move();
}

bool Frame_REQUEST_RESPONSE::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    payload_.deserializeFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(
    std::ostream& os,
    const Frame_REQUEST_RESPONSE& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_REQUEST_FNF::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + payload_.framingSize());
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  payload_.serializeInto(appender);
  return queue.move();
}

bool Frame_REQUEST_FNF::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    payload_.deserializeFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_FNF& frame) {
  return os << frame.header_ << ", " << frame.payload_;
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_METADATA_PUSH::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + sizeof(uint32_t));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  Payload::serializeMetadataInto(appender, std::move(metadata_));
  return queue.move();
}

bool Frame_METADATA_PUSH::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    metadata_ = Payload::deserializeMetadataFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_METADATA_PUSH& frame) {
  return os << frame.header_ << ", "
            << (frame.metadata_ ? frame.metadata_->computeChainDataLength()
                                : 0);
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_CANCEL::serializeOut() {
  auto queue = createBufferQueue(
      FrameHeader::kSize + (metadata_ ? sizeof(uint32_t) : 0));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  Payload::serializeMetadataInto(appender, std::move(metadata_));
  return queue.move();
}

bool Frame_CANCEL::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    metadata_ = Payload::deserializeMetadataFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_CANCEL& frame) {
  return os << frame.header_ << ", "
            << (frame.metadata_ ? frame.metadata_->computeChainDataLength()
                                : 0);
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_RESPONSE::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + payload_.framingSize());
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  payload_.serializeInto(appender);
  return queue.move();
}

bool Frame_RESPONSE::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    payload_.deserializeFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

Frame_RESPONSE Frame_RESPONSE::complete(StreamId streamId) {
  return Frame_RESPONSE(streamId, FrameFlags_COMPLETE, Payload());
};

std::ostream& operator<<(std::ostream& os, const Frame_RESPONSE& frame) {
  return os << frame.header_ << ", (" << frame.payload_;
}
/// @}

/// @{

Frame_ERROR Frame_ERROR::unexpectedFrame() {
  return Frame_ERROR(0, ErrorCode::INVALID, Payload("unexpected frame"));
}

Frame_ERROR Frame_ERROR::badSetupFrame(const std::string& message) {
  return Frame_ERROR(0, ErrorCode::INVALID_SETUP, Payload(message));
}

Frame_ERROR Frame_ERROR::connectionError(const std::string& message) {
  return Frame_ERROR(0, ErrorCode::CONNECTION_ERROR, Payload(message));
}

Frame_ERROR Frame_ERROR::invalid(
    StreamId streamId,
    const std::string& message) {
  return Frame_ERROR(streamId, ErrorCode::INVALID, Payload(message));
}

Frame_ERROR Frame_ERROR::applicationError(
    StreamId streamId,
    const std::string& message) {
  return Frame_ERROR(streamId, ErrorCode::APPLICATION_ERROR, Payload(message));
}

std::unique_ptr<folly::IOBuf> Frame_ERROR::serializeOut() {
  auto queue = createBufferQueue(
      FrameHeader::kSize + sizeof(uint32_t) + payload_.framingSize());
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE(static_cast<uint32_t>(errorCode_));
  payload_.serializeInto(appender);
  return queue.move();
}

bool Frame_ERROR::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    errorCode_ = static_cast<ErrorCode>(cur.readBE<uint32_t>());
    payload_.deserializeFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_ERROR& frame) {
  return os << frame.header_ << ", " << frame.errorCode_ << ", "
            << frame.payload_;
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_KEEPALIVE::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + sizeof(ResumePosition));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  appender.writeBE(position_);
  if (data_) {
    appender.insert(std::move(data_));
  }
  return queue.move();
}

bool Frame_KEEPALIVE::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    assert((header_.flags_ & FrameFlags_METADATA) == 0);
    position_ = cur.readBE<ResumePosition>();
    data_ = Payload::deserializeDataFrom(cur);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_KEEPALIVE& frame) {
  return os << frame.header_ << "(<"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_SETUP::serializeOut() {
  auto queue = createBufferQueue(
      FrameHeader::kSize + 3 * sizeof(uint32_t) + token_.data().size() + 2 +
      metadataMimeType_.length() + dataMimeType_.length() +
      payload_.framingSize());
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE(static_cast<uint32_t>(version_));
  appender.writeBE(static_cast<uint32_t>(keepaliveTime_));
  appender.writeBE(static_cast<uint32_t>(maxLifetime_));
  appender.push((const uint8_t*)token_.data().data(), token_.data().size());

  CHECK(metadataMimeType_.length() <= std::numeric_limits<uint8_t>::max());
  appender.writeBE(static_cast<uint8_t>(metadataMimeType_.length()));
  appender.push(
      (const uint8_t*)metadataMimeType_.data(), metadataMimeType_.length());

  CHECK(dataMimeType_.length() <= std::numeric_limits<uint8_t>::max());
  appender.writeBE(static_cast<uint8_t>(dataMimeType_.length()));
  appender.push((const uint8_t*)dataMimeType_.data(), dataMimeType_.length());

  payload_.serializeInto(appender);
  return queue.move();
}

bool Frame_SETUP::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    version_ = cur.readBE<uint32_t>();
    keepaliveTime_ = cur.readBE<uint32_t>();
    maxLifetime_ = cur.readBE<uint32_t>();
    ResumeIdentificationToken::Data data;
    cur.pull(data.data(), data.size());
    token_.set(std::move(data));

    int mdmtLen = cur.readBE<uint8_t>();
    metadataMimeType_ = cur.readFixedString(mdmtLen);

    int dmtLen = cur.readBE<uint8_t>();
    dataMimeType_ = cur.readFixedString(dmtLen);
    payload_.deserializeFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_SETUP& frame) {
  return os << frame.header_ << ", (" << frame.payload_;
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_LEASE::serializeOut() {
  auto queue = createBufferQueue(
      FrameHeader::kSize + 3 * 2 * sizeof(uint32_t) +
      (metadata_ ? sizeof(uint32_t) : 0));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE(static_cast<uint32_t>(ttl_));
  appender.writeBE(static_cast<uint32_t>(numberOfRequests_));

  Payload::serializeMetadataInto(appender, std::move(metadata_));
  return queue.move();
}

bool Frame_LEASE::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    ttl_ = cur.readBE<uint32_t>();
    numberOfRequests_ = cur.readBE<uint32_t>();
    metadata_ = Payload::deserializeMetadataFrom(cur, header_.flags_);
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_LEASE& frame) {
  return os << frame.header_ << ", ("
            << (frame.metadata_ ? frame.metadata_->computeChainDataLength() : 0)
            << ")";
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_RESUME::serializeOut() {
  auto queue = createBufferQueue(
      FrameHeader::kSize + sizeof(ResumeIdentificationToken) +
      sizeof(ResumePosition));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.push(token_.data().data(), token_.data().size());
  appender.writeBE(position_);

  return queue.move();
}

bool Frame_RESUME::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    ResumeIdentificationToken::Data data;
    cur.pull(data.data(), data.size());
    token_.set(std::move(data));
    position_ = cur.readBE<ResumePosition>();
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_RESUME& frame) {
  return os << frame.header_ << ", ("
            << "token"
            << ", @" << frame.position_ << ")";
}
/// @}

/// @{
std::unique_ptr<folly::IOBuf> Frame_RESUME_OK::serializeOut() {
  auto queue = createBufferQueue(FrameHeader::kSize + sizeof(ResumePosition));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE(position_);

  return queue.move();
}

bool Frame_RESUME_OK::deserializeFrom(std::unique_ptr<folly::IOBuf> in) {
  folly::io::Cursor cur(in.get());
  try {
    header_.deserializeFrom(cur);
    position_ = cur.readBE<ResumePosition>();
  } catch (...) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_RESUME_OK& frame) {
  return os << frame.header_ << ", (@" << frame.position_ << ")";
}
/// @}
}
