// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedReader.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

namespace reactivesocket {

FramedReader::~FramedReader() {}

FramedReader::UniquePtr FramedReader::makeUnique(
    reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>& frames) {
  return UniquePtr(new FramedReader(frames));
}

void FramedReader::onSubscribe(Subscription& subscription) {
  CHECK(!streamSubscription_);
  DestructorGuard dg(this);

  streamSubscription_.reset(&subscription);
  frames_.get()->onSubscribe(*this);
  // no more code here or use DestructorGuard dg(this);
}

void FramedReader::onNext(std::unique_ptr<folly::IOBuf> payload) {
  DestructorGuard dg(this);
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

void FramedReader::onComplete() {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onComplete();
  // no more code here or use DestructorGuard dg(this);
}

void FramedReader::onError(folly::exception_wrapper ex) {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  frames_.onError(std::move(ex));
  // no more code here or use DestructorGuard dg(this);
}

void FramedReader::request(size_t n) {
  DestructorGuard dg(this);
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

void FramedReader::cancel() {
  payloadQueue_.move(); // equivalent to clear(), releases the buffers
  streamSubscription_.cancel();
  // no more code here or use DestructorGuard dg(this);
}

} // reactive socket
