// Copyright 2004-present Facebook. All Rights Reserved.

#include "reactivesocket-cpp/src/Frame.h"

#include <bitset>
#include <memory>
#include <ostream>

#include <folly/Memory.h>
#include <folly/Optional.h>
#include <folly/Singleton.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>

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

void FrameHeader::serializeInto(folly::io::Appender& app) {
  app.writeBE(static_cast<uint16_t>(type_));
  app.writeBE<uint16_t>(flags_);
  app.writeBE<uint32_t>(streamId_);
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
/// @}

/// @{
Payload Frame_REQUEST_SUB::serializeOut() {
  auto buf =
      FrameBufferAllocator::allocate(FrameHeader::kSize + sizeof(uint32_t));

  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  app.writeBE<uint32_t>(requestN_);
  if (data_) {
    buf->appendChain(std::move(data_));
  }
  return buf;
}

bool Frame_REQUEST_SUB::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  // TODO support metadata
  assert((header_.flags_ & FrameFlags_METADATA) == 0);
  try {
    requestN_ = cur.readBE<uint32_t>();
  } catch (...) {
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
  return os << frame.header_ << "(" << frame.requestN_ << ", <"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";
}
/// @}

/// @{
Payload Frame_REQUEST_CHANNEL::serializeOut() {
  auto buf =
      FrameBufferAllocator::allocate(FrameHeader::kSize + sizeof(uint32_t));

  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  app.writeBE<uint32_t>(requestN_);
  if (data_) {
    buf->appendChain(std::move(data_));
  }
  return buf;
}

bool Frame_REQUEST_CHANNEL::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  // TODO support metadata
  assert((header_.flags_ & FrameFlags_METADATA) == 0);
  try {
    requestN_ = cur.readBE<uint32_t>();
  } catch (...) {
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
  return os << frame.header_ << "(" << frame.requestN_ << ", <"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";
}
/// @}

/// @{
Payload Frame_REQUEST_N::serializeOut() {
  auto buf =
      FrameBufferAllocator::allocate(FrameHeader::kSize + sizeof(uint32_t));
  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  app.writeBE<uint32_t>(requestN_);
  return buf;
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
Payload Frame_CANCEL::serializeOut() {
  auto buf = FrameBufferAllocator::allocate(FrameHeader::kSize);
  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  return buf;
}

bool Frame_CANCEL::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  // TODO support metadata
  assert((header_.flags_ & FrameFlags_METADATA) == 0);
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_CANCEL& frame) {
  return os << frame.header_;
}
/// @}

/// @{
Payload Frame_RESPONSE::serializeOut() {
  auto buf = FrameBufferAllocator::allocate(FrameHeader::kSize);

  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  if (data_) {
    buf->appendChain(std::move(data_));
  }
  return buf;
}

bool Frame_RESPONSE::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  // TODO support metadata
  assert((header_.flags_ & FrameFlags_METADATA) == 0);
  auto totalLength = cur.totalLength();
  if (totalLength > 0) {
    cur.clone(data_, totalLength);
  } else {
    data_.reset();
  }
  return true;
}

std::ostream& operator<<(std::ostream& os, const Frame_RESPONSE& frame) {
  return os << frame.header_ << "(<"
            << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
            << ">)";
}
/// @}

/// @{
Payload Frame_ERROR::serializeOut() {
  auto buf =
      FrameBufferAllocator::allocate(FrameHeader::kSize + sizeof(uint32_t));
  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  app.writeBE(static_cast<uint32_t>(errorCode_));
  return buf;
}

bool Frame_ERROR::deserializeFrom(Payload in) {
  folly::io::Cursor cur(in.get());
  if (!header_.deserializeFrom(cur)) {
    return false;
  }
  // TODO support metadata
  assert((header_.flags_ & FrameFlags_METADATA) == 0);
  try {
    errorCode_ = static_cast<ErrorCode>(cur.readBE<uint32_t>());
    return true;
  } catch (...) {
    return false;
  }
}

std::ostream& operator<<(std::ostream& os, const Frame_ERROR& frame) {
  return os << frame.header_ << "(" << frame.errorCode_ << ")";
}
/// @}

/// @{
Payload Frame_KEEPALIVE::serializeOut() {
  auto buf = FrameBufferAllocator::allocate(FrameHeader::kSize);

  folly::io::Appender app(buf.get(), /* do not grow */ 0);
  header_.serializeInto(app);
  if (data_) {
    buf->appendChain(std::move(data_));
  }
  return buf;
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
      auto buf = FrameBufferAllocator::allocate(FrameHeader::kSize + 3 * sizeof(uint32_t));

      folly::io::Appender app(buf.get(), /* do not grow */ 0);
      header_.serializeInto(app);
      app.writeBE(static_cast<uint32_t>(version_));
      app.writeBE(static_cast<uint32_t>(keepaliveTime_));
      app.writeBE(static_cast<uint32_t>(maxLifetime_));
      // TODO encode mime types
      if (data_) {
        buf->appendChain(std::move(data_));
      }
      return buf;
    }

    bool Frame_SETUP::deserializeFrom(Payload in) {
      folly::io::Cursor cur(in.get());
      if (!header_.deserializeFrom(cur)) {
        return false;
      }
      // TODO support metadata
      assert((header_.flags_ & FrameFlags_METADATA) == 0);
      try {
        version_ = cur.readBE<uint32_t>();
        keepaliveTime_ = cur.readBE<uint32_t>();
        maxLifetime_ = cur.readBE<uint32_t>();
      } catch (...) {
        return false;
      }
      // TODO decode mime types
      auto totalLength = cur.totalLength();
      if (totalLength > 0) {
        cur.clone(data_, totalLength);
      } else {
        data_.reset();
      }
      return true;
    }

    std::ostream& operator<<(std::ostream& os, const Frame_SETUP& frame) {
      return os << frame.header_ << "(<"
             << (frame.data_ ? frame.data_->computeChainDataLength() : 0)
             << ">)";
    }
/// @}
}
