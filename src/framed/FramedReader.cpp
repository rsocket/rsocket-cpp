// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/framed/FramedReader.h"
#include <folly/io/Cursor.h>
#include "src/versions/FrameSerializer_v0_1.h"
#include "src/versions/FrameSerializer_v1_0.h"

namespace reactivesocket {
namespace {
constexpr auto kFrameLengthFieldLengthV0_1 = sizeof(int32_t);
constexpr auto kFrameLengthFieldLengthV1_0 = 3; // bytes
} // namespace

size_t FramedReader::getFrameSizeFieldLength() const {
  DCHECK(*protocolVersion_ != ProtocolVersion::Unknown);
  if (*protocolVersion_ < FrameSerializerV1_0::Version) {
    return kFrameLengthFieldLengthV0_1;
  } else {
    return kFrameLengthFieldLengthV1_0; // bytes
  }
}

size_t FramedReader::getFrameMinimalLength() const {
  DCHECK(*protocolVersion_ != ProtocolVersion::Unknown);
  if (*protocolVersion_ < FrameSerializerV1_0::Version) {
    return FrameSerializerV0::kFrameHeaderSize + getFrameSizeFieldLength();
  } else {
    return FrameSerializerV1_0::kFrameHeaderSize;
  }
}

size_t FramedReader::getFrameSizeWithLengthField(size_t frameSize) const {
  DCHECK(*protocolVersion_ != ProtocolVersion::Unknown);
  if (*protocolVersion_ < FrameSerializerV1_0::Version) {
    return frameSize;
  } else {
    return frameSize + getFrameSizeFieldLength();
  }
}

size_t FramedReader::getPayloadSize(size_t frameSize) const {
  DCHECK(*protocolVersion_ != ProtocolVersion::Unknown);
  if (*protocolVersion_ < FrameSerializerV1_0::Version) {
    return frameSize - getFrameSizeFieldLength();
  } else {
    return frameSize;
  }
}

size_t FramedReader::readFrameLength() const {
  auto frameSizeFieldLength = getFrameSizeFieldLength();
  DCHECK(frameSizeFieldLength > 0);

  folly::io::Cursor cur(payloadQueue_.front());
  size_t frameLength = 0;

  // start reading the highest byte
  // frameSizeFieldLength == 3 => shift = [16,8,0]
  // frameSizeFieldLength == 4 => shift = [24,16,8,0]
  auto shift = (frameSizeFieldLength - 1) * 8;

  while (frameSizeFieldLength--) {
    frameLength |= static_cast<size_t>(cur.read<uint8_t>() << shift);
    shift -= 8;
  }
  return frameLength;
}

void FramedReader::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  CHECK(!streamSubscription_);
  streamSubscription_ = std::move(subscription);
  frames_->onSubscribe(shared_from_this());
}

void FramedReader::onNextImpl(std::unique_ptr<folly::IOBuf> payload) noexcept {
  streamRequested_ = false;

  if (payload) {
    VLOG(4) << "incoming bytes length=" << payload->length() << std::endl
            << hexDump(payload->clone()->moveToFbString());
    payloadQueue_.append(std::move(payload));
    parseFrames();
  }
  requestStream();
}

void FramedReader::parseFrames() {
  if (!allowance_.canAcquire() || dispatchingFrames_) {
    return;
  }

  dispatchingFrames_ = true;

  while (allowance_.canAcquire() && frames_) {
    if (!ensureOrAutodetectProtocolVersion()) {
      // at this point we dont have enough bytes onthe wire
      // or we errored out
      break;
    }

    if (payloadQueue_.chainLength() < getFrameSizeFieldLength()) {
      // we don't even have the next frame size value
      break;
    }

    const auto nextFrameSize = readFrameLength();

    // so if the size value is less than minimal frame length something is wrong
    if (nextFrameSize < getFrameMinimalLength()) {
      onErrorImpl(std::runtime_error("invalid data stream"));
      break;
    }

    if (payloadQueue_.chainLength() <
        getFrameSizeWithLengthField(nextFrameSize)) {
      // need to accumulate more data
      break;
    }
    payloadQueue_.trimStart(getFrameSizeFieldLength());
    auto payloadSize = getPayloadSize(nextFrameSize);
    // IOBufQueue::split(0) returns a null unique_ptr, so we create an empty
    // IOBuf object and pass a unique_ptr to it instead. This simplifies
    // clients' code because they can assume the pointer is non-null.
    auto nextFrame = payloadSize != 0 ? payloadQueue_.split(payloadSize)
                                      : folly::IOBuf::create(0);

    CHECK(allowance_.tryAcquire(1));

    VLOG(4) << "parsed frame length=" << nextFrame->length() << std::endl
            << hexDump(nextFrame->clone()->moveToFbString());
    frames_->onNext(std::move(nextFrame));
  }
  dispatchingFrames_ = false;
}

void FramedReader::onCompleteImpl() noexcept {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  if (auto subscriber = std::move(frames_)) {
    subscriber->onComplete();
  }
  if (auto subscription = std::move(streamSubscription_)) {
    subscription->cancel();
  }
}

void FramedReader::onErrorImpl(folly::exception_wrapper ex) noexcept {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  if (auto subscriber = std::move(frames_)) {
    subscriber->onError(std::move(ex));
  }
  if (auto subscription = std::move(streamSubscription_)) {
    subscription->cancel();
  }
}

void FramedReader::requestImpl(size_t n) noexcept {
  allowance_.release(n);
  parseFrames();
  requestStream();
}

void FramedReader::requestStream() {
  if (streamSubscription_ && allowance_.canAcquire()) {
    streamRequested_ = true;
    streamSubscription_->request(1);
  }
}

void FramedReader::cancelImpl() noexcept {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  if (auto subscription = std::move(streamSubscription_)) {
    subscription->cancel();
  }
  if (auto subscriber = std::move(frames_)) {
    subscriber->onComplete();
  }
}

bool FramedReader::ensureOrAutodetectProtocolVersion() {
  if (*protocolVersion_ != ProtocolVersion::Unknown) {
    return true;
  }

  auto minBytesNeeded = std::max(
      FrameSerializerV0_1::kMinBytesNeededForAutodetection,
      FrameSerializerV1_0::kMinBytesNeededForAutodetection);
  DCHECK(minBytesNeeded > 0);
  if (payloadQueue_.chainLength() < minBytesNeeded) {
    return false;
  }

  DCHECK(minBytesNeeded > kFrameLengthFieldLengthV0_1);
  DCHECK(minBytesNeeded > kFrameLengthFieldLengthV1_0);

  bool recognized = FrameSerializerV1_0::detectProtocolVersion(
                        *payloadQueue_.front(), kFrameLengthFieldLengthV1_0) !=
      ProtocolVersion::Unknown;
  if (recognized) {
    *protocolVersion_ = FrameSerializerV1_0::Version;
    return true;
  }

  recognized = FrameSerializerV0_1::detectProtocolVersion(
                   *payloadQueue_.front(), kFrameLengthFieldLengthV0_1) !=
      ProtocolVersion::Unknown;
  if (recognized) {
    *protocolVersion_ = FrameSerializerV0_1::Version;
    return true;
  }

  onErrorImpl(
      std::runtime_error("could not detect protocol version from framing"));
  return false;
}

} // reactivesocket
