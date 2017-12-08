// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FramedDuplexConnection.h"

#include <folly/io/Cursor.h>

#include "rsocket/framing/FrameSerializer.h"
#include "rsocket/framing/FrameSerializer_v1_0.h"
#include "rsocket/framing/FramedReader.h"

namespace rsocket {

using namespace yarpl::flowable;

namespace {

constexpr auto kMaxFrameLength = 0xFFFFFF; // 24bit max value

template <typename TWriter>
void writeFrameLength(
    TWriter& cur,
    size_t frameLength,
    size_t frameSizeFieldLength) {
  DCHECK(frameSizeFieldLength > 0);

  // starting from the highest byte
  // frameSizeFieldLength == 3 => shift = [16,8,0]
  // frameSizeFieldLength == 4 => shift = [24,16,8,0]
  auto shift = (frameSizeFieldLength - 1) * 8;

  while (frameSizeFieldLength--) {
    auto byte = (frameLength >> shift) & 0xFF;
    cur.write(static_cast<uint8_t>(byte));
    shift -= 8;
  }
}

size_t getFrameSizeFieldLength(ProtocolVersion version) {
  CHECK(version != ProtocolVersion::Unknown);
  if (version < FrameSerializerV1_0::Version) {
    return sizeof(int32_t);
  } else {
    return 3; // bytes
  }
}

size_t getPayloadLength(ProtocolVersion version, size_t payloadLength) {
  DCHECK(version != ProtocolVersion::Unknown);
  if (version < FrameSerializerV1_0::Version) {
    return payloadLength + getFrameSizeFieldLength(version);
  } else {
    return payloadLength;
  }
}

std::unique_ptr<folly::IOBuf> prependSize(
    ProtocolVersion version,
    std::unique_ptr<folly::IOBuf> payload) {
  CHECK(payload);

  const auto frameSizeFieldLength = getFrameSizeFieldLength(version);
  // the frame size includes the payload size and the size value
  auto payloadLength =
      getPayloadLength(version, payload->computeChainDataLength());
  if (payloadLength > kMaxFrameLength) {
    return nullptr;
  }

  if (payload->headroom() >= frameSizeFieldLength) {
    // move the data pointer back and write value to the payload
    payload->prepend(frameSizeFieldLength);
    folly::io::RWPrivateCursor cur(payload.get());
    writeFrameLength(cur, payloadLength, frameSizeFieldLength);
    VLOG(4) << "writing frame length=" << payload->length() << std::endl
            << hexDump(payload->clone()->moveToFbString());
    return payload;
  } else {
    auto newPayload = folly::IOBuf::createCombined(frameSizeFieldLength);
    folly::io::Appender appender(newPayload.get(), /* do not grow */ 0);
    writeFrameLength(appender, payloadLength, frameSizeFieldLength);
    newPayload->appendChain(std::move(payload));
    VLOG(4) << "writing frame length=" << newPayload->computeChainDataLength()
            << std::endl
            << hexDump(newPayload->clone()->moveToFbString());
    return newPayload;
  }
}

} // namespace

FramedDuplexConnection::~FramedDuplexConnection() {}

FramedDuplexConnection::FramedDuplexConnection(
    std::unique_ptr<DuplexConnection> connection,
    ProtocolVersion protocolVersion)
    : inner_(std::move(connection)),
      protocolVersion_(std::make_shared<ProtocolVersion>(protocolVersion)) {}

void FramedDuplexConnection::send(std::unique_ptr<folly::IOBuf> buf) {
  if (!inner_) {
    return;
  }

  auto sized = prependSize(*protocolVersion_, std::move(buf));
  if (!sized) {
    protocolVersion_.reset();
    inputReader_.reset();
    inner_.reset();
    return;
  }

  inner_->send(std::move(sized));
}

void FramedDuplexConnection::setInput(
    yarpl::Reference<DuplexConnection::Subscriber> framesSink) {
  if (!inputReader_) {
    inputReader_ = yarpl::make_ref<FramedReader>(protocolVersion_);
    inner_->setInput(inputReader_);
  }
  inputReader_->setInput(std::move(framesSink));
}
} // namespace rsocket
