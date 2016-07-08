// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Frame.h"

#include <bitset>
#include <memory>
#include <ostream>

#include <folly/Memory.h>
#include <folly/Optional.h>
#include <folly/Singleton.h>
#include <folly/io/Cursor.h>

namespace {
folly::Singleton<reactivesocket::FrameBufferAllocator> bufferAllocatorSingleton;
}

// TODO(stupaq): strict enum validation
// TODO(stupaq): verify whether frames contain extra data
// TODO(stupaq): get rid of these try-catch blocks
namespace reactivesocket {

std::unique_ptr<folly::IOBuf> FrameBufferAllocator::allocate(size_t size) {
  return folly::Singleton<FrameBufferAllocator>::get()->allocateBuffer(size);
}

std::unique_ptr<folly::IOBuf> FrameBufferAllocator::allocateBuffer(
    size_t size) {
  return folly::IOBuf::createCombined(size);
}

std::ostream& operator<<(std::ostream& os, FrameType type) {
  switch (type) {
    case FrameType::REQUEST_SUB:
      return os << "REQUEST_SUB";
    case FrameType::REQUEST_CHANNEL:
      return os << "REQUEST_CHANNEL";
    case FrameType::REQUEST_N:
      return os << "REQUEST_N";
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
  }
  // this should be never hit because the switch is over all cases
  std::abort();
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
  }
  // this should be never hit because the switch is over all cases
  std::abort();
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

bool FrameHeader::deserializeFrom(folly::io::Cursor& cur) {
  try {
    type_ = static_cast<FrameType>(cur.readBE<uint16_t>());
    flags_ = cur.readBE<uint16_t>();
    streamId_ = cur.readBE<uint32_t>();
    return true;
  } catch (...) {
    return false;
  }
}

std::ostream& operator<<(std::ostream& os, const FrameHeader& header) {
  std::bitset<16> flags(header.flags_);
  return os << header.type_ << "[" << flags << ", " << header.streamId_ << "]";
}

constexpr auto kMaxMetaLength = std::numeric_limits<int32_t>::max();

FrameMetadata FrameMetadata::empty() {
  return FrameMetadata();
}

void FrameMetadata::checkFlags(FrameFlags flags) {
  const bool metadataPresent = (flags & FrameFlags_METADATA) != 0;
  assert(metadataPresent == (metadataPayload_ != nullptr));
}

void FrameMetadata::serializeInto(folly::io::QueueAppender& appender) {
  if (metadataPayload_ != nullptr) {
    // use signed int because the first bit in metadata length is reserved
    assert(metadataPayload_->length() + sizeof(uint32_t) < kMaxMetaLength);

    appender.writeBE<uint32_t>(static_cast<uint32_t>(metadataPayload_->length()) + sizeof(uint32_t));
    appender.insert(std::move(metadataPayload_));
  }
}

bool FrameMetadata::deserializeFrom(folly::io::Cursor& cur,
                                    const FrameFlags& flags,
                                    FrameMetadata& metadata) {
  if (flags & FrameFlags_METADATA) {
    FrameMetadata m;
    if (!m.deserializeFrom(cur)) {
      return false;
    } else {
      metadata = std::move(m);
      return true;
    }
  } else { // no metadata was set
    return true;
  }
}

bool FrameMetadata::deserializeFrom(folly::io::Cursor& cur) {
  try {
    const auto length = cur.readBE<uint32_t>();
    assert(length < kMaxMetaLength);
    const auto metadataPayloadLength = length - sizeof(uint32_t);
    
    if (metadataPayloadLength > 0) {
      cur.clone(metadataPayload_, metadataPayloadLength);
    } else {
      metadataPayload_.reset();
    }
    return true;
  } catch (...) {
    return false;
  }
}

std::ostream& operator<<(std::ostream& os, const FrameMetadata& metadata) {
  return os << "[meta: " << (metadata.metadataPayload_ ?
                              folly::to<std::string>(metadata.metadataPayload_->computeChainDataLength())
                              : "empty") << "]";
}
/// @}

/// @{
Payload Frame_REQUEST_SUB::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const bool metadataPresent = (header_.flags_ & FrameFlags_METADATA) != 0;
  const auto bufSize = FrameHeader::kSize + sizeof(uint32_t) +
    (metadataPresent ? sizeof(uint32_t) : 0);
  auto buf =
    FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  appender.writeBE<uint32_t>(requestN_);
  metadata_.serializeInto(appender);
  if (data_) {
    appender.insert(std::move(data_));
  }
  return queue.move();
}

bool Frame_REQUEST_SUB::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  try {
    requestN_ = cur.readBE<uint32_t>();
  } catch (...) {
    return false;
  }
  if (!FrameMetadata::deserializeFrom(cur, header_.flags_, metadata_)) {
    return false;
  }
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_SUB& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ", " << frame.metadata_ << ", <" <<
            (frame.data_ ? frame.data_->computeChainDataLength() : 0) << ">)";
}
/// @}

/// @{
Payload Frame_REQUEST_CHANNEL::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const bool metadataPresent = (header_.flags_ & FrameFlags_METADATA) != 0;
  const auto bufSize = FrameHeader::kSize + sizeof(uint32_t) +
    (metadataPresent ? sizeof(uint32_t) : 0);
  auto buf =
    FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE<uint32_t>(requestN_);
  metadata_.serializeInto(appender);
  if (data_) {
    appender.insert(std::move(data_));
  }

  return queue.move();
}

bool Frame_REQUEST_CHANNEL::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  try {
    requestN_ = cur.readBE<uint32_t>();
  } catch (...) {
    return false;
  }
  if (!FrameMetadata::deserializeFrom(cur, header_.flags_, metadata_)) {
    return false;
  }
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_CHANNEL& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ", " << frame.metadata_ << ", <"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0) << ">)";
}
/// @}

/// @{
Payload Frame_REQUEST_N::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const auto bufSize = FrameHeader::kSize + sizeof(uint32_t);
  auto buf = FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
    folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  appender.writeBE<uint32_t>(requestN_);
  return queue.move();
}

bool Frame_REQUEST_N::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  try {
    requestN_ = cur.readBE<uint32_t>();
    return true;
  } catch (...) {
    return false;
  }
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_N& frame) {
  return os << frame.header_ << "(" << frame.requestN_ << ")";
}
/// @}

/// @{
Payload Frame_REQUEST_FNF::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const auto metadataPresent = header_.flags_ & FrameFlags_METADATA;
  const auto bufSize = FrameHeader::kSize + sizeof(uint32_t) +
                       (metadataPresent ? sizeof(uint32_t) : 0);
  auto buf = FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  if (metadataPresent && metadata_.metadataPayload_) {
    metadata_.serializeInto(appender);
  }
  if (data_) {
    appender.insert(std::move(data_));
  }
  return queue.move();
}

bool Frame_REQUEST_FNF::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  if (!FrameMetadata::deserializeFrom(cur, header_.flags_, metadata_)) {
    return false;
  }
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_REQUEST_FNF& frame) {
  return os << frame.header_ << ", " << frame.metadata_ << ", <"
    << (frame.data_ ? frame.data_->computeChainDataLength() : 0) << ">)";
}
/// @}

/// @{
Payload Frame_CANCEL::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const bool metadataPresent = (header_.flags_ & FrameFlags_METADATA) != 0;
  const auto bufSize = FrameHeader::kSize +
    (metadataPresent ? sizeof(uint32_t) : 0);
  auto buf = FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  metadata_.serializeInto(appender);
  return queue.move();
}

bool Frame_CANCEL::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  if (!FrameMetadata::deserializeFrom(cur, header_.flags_, metadata_)) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_CANCEL& frame) {
  return os << frame.header_ << ", " << frame.metadata_;
}
/// @}

/// @{
Payload Frame_RESPONSE::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const bool metadataPresent = (header_.flags_ & FrameFlags_METADATA) != 0;
  const auto bufSize = FrameHeader::kSize +
    (metadataPresent ? sizeof(uint32_t) : 0);
  auto buf = FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  metadata_.serializeInto(appender);
  if (data_) {
    appender.insert(std::move(data_));
  }
  return queue.move();
}

bool Frame_RESPONSE::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  if (!FrameMetadata::deserializeFrom(cur, header_.flags_, metadata_)) {
    return false;
  }
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_RESPONSE& frame) {
  return os << frame.header_ << ", (" << frame.metadata_ << " <"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";
}
/// @}

/// @{
Payload Frame_ERROR::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  const bool metadataPresent = (header_.flags_ & FrameFlags_METADATA) != 0;
  const auto bufSize = FrameHeader::kSize + sizeof(uint32_t) +
    (metadataPresent ? sizeof(uint32_t) : 0);
  auto buf = FrameBufferAllocator::allocate(bufSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  appender.writeBE(static_cast<uint32_t>(errorCode_));
  metadata_.serializeInto(appender);
  return queue.move();
}

bool Frame_ERROR::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  try {
    errorCode_ = static_cast<ErrorCode>(cur.readBE<uint32_t>());
  } catch (...) {
    return false;
  }
  if (!FrameMetadata::deserializeFrom(cur, header_.flags_, metadata_)) {
    return false;
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_ERROR& frame) {
  return os << frame.header_ << ", " << frame.metadata_ << ", (" << frame.errorCode_ << ")";
}
/// @}

/// @{
Payload Frame_KEEPALIVE::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  auto buf = FrameBufferAllocator::allocate(FrameHeader::kSize);
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  header_.serializeInto(appender);
  if (data_) {
    appender.insert(std::move(data_));
  }
  return queue.move();
}

bool Frame_KEEPALIVE::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  assert((header_.flags_ & FrameFlags_METADATA) == 0);
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
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
Payload Frame_SETUP::serializeOut() {
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  auto buf = FrameBufferAllocator::allocate(FrameHeader::kSize + 3 * sizeof(uint32_t));
  queue.append(std::move(buf));
  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);

  header_.serializeInto(appender);
  appender.writeBE(static_cast<uint32_t>(version_));
  appender.writeBE(static_cast<uint32_t>(keepaliveTime_));
  appender.writeBE(static_cast<uint32_t>(maxLifetime_));
  // TODO encode mime types
  // TODO encode metadata
  if (data_) {
    appender.insert(std::move(data_));
  }
  return queue.move();
}

bool Frame_SETUP::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  try {
    version_ = cur.readBE<uint32_t>();
    keepaliveTime_ = cur.readBE<uint32_t>();
    maxLifetime_ = cur.readBE<uint32_t>();
  } catch (...) {
    return false;
  }
  // TODO decode mime types
  // TODO decode metadata
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_SETUP& frame) {
  return os << frame.header_ << ", (" << frame.metadata_ << ", <"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";;
}
/// @}
}
