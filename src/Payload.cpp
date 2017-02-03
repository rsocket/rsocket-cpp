// Copyright 2004-present Facebook. All Rights Reserved.

#include "Payload.h"

#include <folly/String.h>
#include <folly/io/Cursor.h>

namespace reactivesocket {

constexpr auto kMaxMetadataLength = std::numeric_limits<int32_t>::max();

Payload::Payload(
    std::unique_ptr<folly::IOBuf> _data,
    std::unique_ptr<folly::IOBuf> _metadata)
    : data(std::move(_data)), metadata(std::move(_metadata)) {}

Payload::Payload(const std::string& _data, const std::string& _metadata)
  : data(folly::IOBuf::copyBuffer(_data)) {
  if (!_metadata.empty()) {
    metadata = folly::IOBuf::copyBuffer(_metadata);
  }
}


void Payload::checkFlags(FrameFlags flags) const {
  assert(bool(flags & FrameFlags_METADATA) == bool(metadata));
}

void Payload::serializeMetadataInto(
    folly::io::QueueAppender& appender,
    std::unique_ptr<folly::IOBuf> metadata) {
  if (metadata == nullptr) {
    return;
  }

  // Use signed int because the first bit in metadata length is reserved.
  if (metadata->length() >= kMaxMetadataLength - sizeof(uint32_t)) {
    throw std::runtime_error("Metadata is too big to serialize");
  }

  appender.writeBE<uint32_t>(
    static_cast<uint32_t>(metadata->length() + sizeof(uint32_t)));
  appender.insert(std::move(metadata));
}

void Payload::serializeInto(folly::io::QueueAppender& appender) {
  serializeMetadataInto(appender, std::move(metadata));
  if (data) {
    appender.insert(std::move(data));
  }
}

std::unique_ptr<folly::IOBuf> Payload::deserializeMetadataFrom(
    folly::io::Cursor& cur,
    FrameFlags flags) {
  if ((flags & FrameFlags_METADATA) == 0) {
    return nullptr;
  }

  const auto length = cur.readBE<uint32_t>();

  if (length >= kMaxMetadataLength) {
    throw std::runtime_error("Metadata is too big to deserialize");
  }

  if (length <= sizeof(uint32_t)) {
    throw std::runtime_error("Metadata is too small to encode its size");
  }

  const auto metadataPayloadLength =
    length - static_cast<uint32_t>(sizeof(uint32_t));

  // TODO: Check if metadataPayloadLength exceeds frame length minus frame
  // header size.

  std::unique_ptr<folly::IOBuf> metadata;
  cur.clone(metadata, metadataPayloadLength);
  return metadata;
}

std::unique_ptr<folly::IOBuf> Payload::deserializeDataFrom(
    folly::io::Cursor& cur) {
  std::unique_ptr<folly::IOBuf> data;
  auto totalLength = cur.totalLength();

  if (totalLength > 0) {
    cur.clone(data, totalLength);
  }
  return data;
}

void Payload::deserializeFrom(folly::io::Cursor& cur, FrameFlags flags) {
  metadata = deserializeMetadataFrom(cur, flags);
  data = deserializeDataFrom(cur);
}

std::ostream& operator<<(std::ostream& os, const Payload& payload) {
  return os << "[metadata: "
            << (payload.metadata
                    ? folly::to<std::string>(
                          payload.metadata->computeChainDataLength())
                    : "<null>")
            << " data: " << (payload.data
                                 ? folly::to<std::string>(
                                       payload.data->computeChainDataLength())
                                 : "<null>")
            << "]";
}

std::string Payload::moveDataToString() {
  if (!data) {
    return "";
  }
  return data->moveToFbString().toStdString();
}

void Payload::clear() {
  data.reset();
  metadata.reset();
}

} // reactivesocket
