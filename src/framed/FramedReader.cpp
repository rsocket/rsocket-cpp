// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedReader.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

namespace reactivesocket {

void FramedReader::onSubscribeImpl(std::shared_ptr<Subscription> subscription) {
  CHECK(!streamSubscription_);
  streamSubscription_.reset(std::move(subscription));
  frames_.onSubscribe(shared_from_this());
}

void FramedReader::onNextImpl(std::unique_ptr<folly::IOBuf> payload) {
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
    const auto nextFrameSize = c.readBE<int32_t>();
    CHECK_GE(nextFrameSize, sizeof(int32_t));

    if (payloadQueue_.chainLength() < (size_t)nextFrameSize) {
      // need to accumulate more data
      break;
    }
    payloadQueue_.trimStart(sizeof(int32_t));
    // the frame size includes the payload size and the size value
    auto nextFrame = payloadQueue_.split(nextFrameSize - sizeof(int32_t));

    CHECK(allowance_.tryAcquire(1));
    frames_.onNext(std::move(nextFrame));
  }
  dispatchingFrames_ = false;
}

void FramedReader::onCompleteImpl() {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onComplete();
  streamSubscription_.cancel();
}

void FramedReader::onErrorImpl(folly::exception_wrapper ex) {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onError(std::move(ex));
  streamSubscription_.cancel();
}

void FramedReader::requestImpl(size_t n) {
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

void FramedReader::cancelImpl() {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  streamSubscription_.cancel();
  frames_.onComplete();
}

} // reactive socket
