// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedReader.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

namespace reactivesocket {

void FramedReader::onSubscribe(Subscription& subscription) {
  CHECK(!streamSubscription_);
  streamSubscription_.reset(&subscription);
  frames_.get()->onSubscribe(*this);
}

void FramedReader::onNext(std::unique_ptr<folly::IOBuf> payload) {
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

    auto type = FrameHeader::peekType(*nextFrame);

    CHECK(allowance_.tryAcquire(1));
    frames_.onNext(std::move(nextFrame));

    stats_.frameRead(type);
  }
  dispatchingFrames_ = false;
}

void FramedReader::onComplete() {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onComplete();
}

void FramedReader::onError(folly::exception_wrapper ex) {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onError(std::move(ex));
}

void FramedReader::request(size_t n) {
  allowance_.release(n);
  parseFrames();
  requestStream();
}

void FramedReader::requestStream() {
  if (allowance_.canAcquire()) {
    streamRequested_ = true;
    streamSubscription_.request(1);
  }
}

void FramedReader::cancel() {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  streamSubscription_.cancel();
}

} // reactive socket
