// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedReader.h"

#include <folly/io/Cursor.h>

namespace reactivesocket {

void FramedReader::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  CHECK(!streamSubscription_);
  streamSubscription_.reset(std::move(subscription));
  frames_.onSubscribe(shared_from_this());
}

void FramedReader::onNextImpl(std::unique_ptr<folly::IOBuf> payload) noexcept {
  streamRequested_ = false;

  if (payload) {
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
    if (payloadQueue_.chainLength() < sizeof(int32_t)) {
      // we don't even have the next frame size value
      break;
    }

    folly::io::Cursor c(payloadQueue_.front());
    const auto nextFrameSize = static_cast<size_t>(c.readBE<int32_t>());

    // the frame size includes the payload size and the size value
    // so if the size value is less than sizeof(int32_t) something is wrong
    if (nextFrameSize < sizeof(int32_t)) {
      onErrorImpl(std::runtime_error("invalid data stream"));
      break;
    }

    if (payloadQueue_.chainLength() < nextFrameSize) {
      // need to accumulate more data
      break;
    }
    payloadQueue_.trimStart(sizeof(int32_t));
    auto payloadSize = nextFrameSize - sizeof(int32_t);
    // IOBufQueue::split(0) returns a null unique_ptr, so we create an empty
    // IOBuf object and pass a unique_ptr to it instead. This simplifies
    // clients' code because they can assume the pointer is non-null.
    auto nextFrame = payloadSize != 0 ?
      payloadQueue_.split(payloadSize) : folly::IOBuf::create(0);

    CHECK(allowance_.tryAcquire(1));
    frames_.onNext(std::move(nextFrame));
  }
  dispatchingFrames_ = false;
}

void FramedReader::onCompleteImpl() noexcept {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onComplete();
  streamSubscription_.cancel();
}

void FramedReader::onErrorImpl(folly::exception_wrapper ex) noexcept {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onError(std::move(ex));
  streamSubscription_.cancel();
}

void FramedReader::requestImpl(size_t n) noexcept {
  allowance_.release(n);
  parseFrames();
  requestStream();
}

void FramedReader::requestStream() {
  if (streamSubscription_ && allowance_.canAcquire()) {
    streamRequested_ = true;
    streamSubscription_.request(1);
  }
}

void FramedReader::cancelImpl() noexcept {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  streamSubscription_.cancel();
  frames_.onComplete();
}

} // reactivesocket
