// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/framing/FramedReader.h"

#include <folly/io/Cursor.h>

#include "rsocket/framing/FrameSerializer_v0_1.h"
#include "rsocket/framing/FrameSerializer_v1_0.h"

namespace rsocket {

using namespace yarpl::flowable;

namespace {

constexpr size_t kFrameLengthFieldLengthV0_1 = sizeof(int32_t);
constexpr size_t kFrameLengthFieldLengthV1_0 = 3;

/// Get the byte size of the frame length field in an RSocket frame.
size_t frameSizeFieldLength(ProtocolVersion version) {
  DCHECK_NE(version, ProtocolVersion::Unknown);
  return version < FrameSerializerV1_0::Version ? kFrameLengthFieldLengthV0_1
                                                : kFrameLengthFieldLengthV1_0;
}

/// Get the minimum size for a valid RSocket frame (including its frame length
/// field).
size_t minimalFrameLength(ProtocolVersion version) {
  DCHECK_NE(version, ProtocolVersion::Unknown);
  return version < FrameSerializerV1_0::Version
      ? FrameSerializerV0::kFrameHeaderSize + frameSizeFieldLength(version)
      : FrameSerializerV1_0::kFrameHeaderSize;
}

/// Compute the length of the entire frame (including its frame length field),
/// if given only its frame length field.
size_t frameSizeWithLengthField(ProtocolVersion version, size_t frameSize) {
  return version < FrameSerializerV1_0::Version
      ? frameSize
      : frameSize + frameSizeFieldLength(version);
}

/// Compute the length of the frame (excluding its frame length field), if given
/// only its frame length field.
size_t frameSizeWithoutLengthField(ProtocolVersion version, size_t frameSize) {
  DCHECK_NE(version, ProtocolVersion::Unknown);
  return version < FrameSerializerV1_0::Version
      ? frameSize - frameSizeFieldLength(version)
      : frameSize;
}
}

size_t FramedReader::readFrameLength() const {
  auto fieldLength = frameSizeFieldLength(*version_);
  DCHECK_GT(fieldLength, 0);

  folly::io::Cursor cur{payloadQueue_.front()};
  size_t frameLength = 0;

  // Reading of arbitrary-sized big-endian integer.
  for (size_t i = 0; i < fieldLength; ++i) {
    frameLength <<= 8;
    frameLength |= cur.read<uint8_t>();
  }

  return frameLength;
}

void FramedReader::onSubscribe(yarpl::Reference<Subscription> subscription) {
  DuplexConnection::Subscriber::onSubscribe(subscription);
  subscription->request(std::numeric_limits<int64_t>::max());
}

void FramedReader::onNext(std::unique_ptr<folly::IOBuf> payload) {
  VLOG(4) << "incoming bytes length=" << payload->length() << '\n'
          << hexDump(payload->clone()->moveToFbString());
  payloadQueue_.append(std::move(payload));
  parseFrames();
}

void FramedReader::parseFrames() {
  if (dispatchingFrames_) {
    return;
  }

  // Delivering onNext can trigger termination and destroy this instance.
  auto thisPtr = this->ref_from_this(this);

  dispatchingFrames_ = true;

  while (allowance_.canAcquire() && inner_) {
    if (!ensureOrAutodetectProtocolVersion()) {
      // At this point we dont have enough bytes on the wire or we errored out.
      break;
    }

    auto const frameSizeFieldLen = frameSizeFieldLength(*version_);
    if (payloadQueue_.chainLength() < frameSizeFieldLen) {
      // We don't even have the next frame size value.
      break;
    }

    auto const nextFrameSize = readFrameLength();
    if (nextFrameSize < minimalFrameLength(*version_)) {
      error("Invalid frame - Frame size smaller than minimum");
      break;
    }

    if (payloadQueue_.chainLength() <
        frameSizeWithLengthField(*version_, nextFrameSize)) {
      // Need to accumulate more data.
      break;
    }

    payloadQueue_.trimStart(frameSizeFieldLen);
    auto payloadSize = frameSizeWithoutLengthField(*version_, nextFrameSize);

    DCHECK_GT(payloadSize, 0)
        << "folly::IOBufQueue::split(0) returns a nullptr, can't have that";
    auto nextFrame = payloadQueue_.split(payloadSize);

    CHECK(allowance_.tryAcquire(1));

    VLOG(4) << "parsed frame length=" << nextFrame->length() << '\n'
            << hexDump(nextFrame->clone()->moveToFbString());
    inner_->onNext(std::move(nextFrame));
  }

  dispatchingFrames_ = false;
}

void FramedReader::onComplete() {
  payloadQueue_.move();
  if (DuplexConnection::Subscriber::subscription()) {
    DuplexConnection::Subscriber::onComplete();
  }
  if (auto subscriber = std::move(inner_)) {
    // After this call the instance can be destroyed!
    subscriber->onComplete();
  }
}

void FramedReader::onError(folly::exception_wrapper ex) {
  payloadQueue_.move();
  if (DuplexConnection::Subscriber::subscription()) {
    DuplexConnection::Subscriber::onError({});
  }
  if (auto subscriber = std::move(inner_)) {
    // After this call the instance can be destroyed!
    subscriber->onError(std::move(ex));
  }
}

void FramedReader::request(int64_t n) {
  allowance_.release(n);
  parseFrames();
}

void FramedReader::cancel() {
  allowance_.drain();
  inner_ = nullptr;
}

void FramedReader::setInput(
    yarpl::Reference<DuplexConnection::Subscriber> inner) {
  CHECK(!inner_)
      << "Must cancel original input to FramedReader before setting a new one";
  inner_ = std::move(inner);
  inner_->onSubscribe(this->ref_from_this(this));
}

bool FramedReader::ensureOrAutodetectProtocolVersion() {
  if (*version_ != ProtocolVersion::Unknown) {
    return true;
  }

  auto minBytesNeeded = std::max(
      FrameSerializerV0_1::kMinBytesNeededForAutodetection,
      FrameSerializerV1_0::kMinBytesNeededForAutodetection);
  DCHECK_GT(minBytesNeeded, 0);
  if (payloadQueue_.chainLength() < minBytesNeeded) {
    return false;
  }

  DCHECK_GT(minBytesNeeded, kFrameLengthFieldLengthV0_1);
  DCHECK_GT(minBytesNeeded, kFrameLengthFieldLengthV1_0);

  auto const& firstFrame = *payloadQueue_.front();

  auto detected = FrameSerializerV1_0::detectProtocolVersion(
      firstFrame, kFrameLengthFieldLengthV1_0);
  if (detected != ProtocolVersion::Unknown) {
    *version_ = FrameSerializerV1_0::Version;
    return true;
  }

  detected = FrameSerializerV0_1::detectProtocolVersion(
      firstFrame, kFrameLengthFieldLengthV0_1);
  if (detected != ProtocolVersion::Unknown) {
    *version_ = FrameSerializerV0_1::Version;
    return true;
  }

  error("Could not detect protocol version from framing");
  return false;
}

void FramedReader::error(std::string errorMsg) {
  VLOG(1) << "error: " << errorMsg;

  if (DuplexConnection::Subscriber::subscription()) {
    onError(std::runtime_error(std::move(errorMsg)));
  }

  if (DuplexConnection::Subscriber::subscription()) {
    DuplexConnection::Subscriber::subscription()->cancel();
  }
}
}
