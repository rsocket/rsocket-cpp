// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedWriter.h"

#include <folly/ExceptionWrapper.h>
#include <folly/Optional.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

namespace reactivesocket {

void FramedWriter::onSubscribe(Subscription& subscription) {
  CHECK(!writerSubscription_);
  writerSubscription_.reset(&subscription);
  stream_.get()->onSubscribe(*this);
}

void FramedWriter::onNext(std::unique_ptr<folly::IOBuf> payload) {
  CHECK(payload);

  // the frame size includes the payload size and the size value
  auto payloadLength = payload->computeChainDataLength() + sizeof(int32_t);
  if (payloadLength > std::numeric_limits<int32_t>::max()) {
    VLOG(1) << "payload too big: " << payloadLength;
    cancel();
    return;
  }

  auto type = FrameHeader::peekType(*payload);

  if (payload->headroom() >= sizeof(int32_t)) {
    // move the data pointer back and write value to the payload
    payload->prepend(sizeof(int32_t));
    folly::io::RWPrivateCursor c(payload.get());
    c.writeBE<int32_t>(payloadLength);
    stream_.onNext(std::move(payload));
  } else {
    auto newPayload = folly::IOBuf::createCombined(sizeof(int32_t));
    folly::io::Appender appender(newPayload.get(), /* do not grow */ 0);
    appender.writeBE<int32_t>(payloadLength);
    newPayload->appendChain(std::move(payload));
    stream_.onNext(std::move(newPayload));
  }

  stats_.frameWritten(type);
}

void FramedWriter::onComplete() {
  stream_.onComplete();
}

void FramedWriter::onError(folly::exception_wrapper ex) {
  stream_.onError(std::move(ex));
}

void FramedWriter::request(size_t n) {
  writerSubscription_.request(n);
}

void FramedWriter::cancel() {
  writerSubscription_.cancel();
}

} // reactive socket
