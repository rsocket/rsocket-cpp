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

static std::unique_ptr<folly::IOBuf> appendSize(
    std::unique_ptr<folly::IOBuf> payload) {
  CHECK(payload);

  // the frame size includes the payload size and the size value
  auto payloadLength = payload->computeChainDataLength() + sizeof(int32_t);
  if (payloadLength > std::numeric_limits<int32_t>::max()) {
    return nullptr;
  }

  if (payload->headroom() >= sizeof(int32_t)) {
    // move the data pointer back and write value to the payload
    payload->prepend(sizeof(int32_t));
    folly::io::RWPrivateCursor c(payload.get());
    c.writeBE<int32_t>(payloadLength);
    return payload;
  } else {
    auto newPayload = folly::IOBuf::createCombined(sizeof(int32_t));
    folly::io::Appender appender(newPayload.get(), /* do not grow */ 0);
    appender.writeBE<int32_t>(payloadLength);
    newPayload->appendChain(std::move(payload));
    return newPayload;
  }
}

void FramedWriter::onNext(std::unique_ptr<folly::IOBuf> payload) {
  auto sizedPayload = appendSize(std::move(payload));
  if (!sizedPayload) {
    VLOG(1) << "payload too big";
    cancel();
    return;
  }
  stream_.onNext(std::move(sizedPayload));
}

void FramedWriter::onNextMultiple(
    std::vector<std::unique_ptr<folly::IOBuf>> payloads) {
  folly::IOBufQueue payloadQueue;

  for (auto& payload : payloads) {
    auto sizedPayload = appendSize(std::move(payload));
    if (!sizedPayload) {
      VLOG(1) << "payload too big";
      cancel();
      return;
    }
    payloadQueue.append(std::move(sizedPayload));
  }
  stream_.onNext(payloadQueue.move());
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
