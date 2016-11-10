// Copyright 2004-present Facebook. All Rights Reserved.
#include "FramedWriter.h"

#include <folly/ExceptionWrapper.h>
#include <folly/Optional.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

namespace reactivesocket {

void FramedWriter::onSubscribeImpl(std::shared_ptr<Subscription> subscription) {
  CHECK(!writerSubscription_);
  writerSubscription_.reset(std::move(subscription));
  stream_.onSubscribe(shared_from_this());
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

void FramedWriter::onNextImpl(std::unique_ptr<folly::IOBuf> payload) {
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

void FramedWriter::onCompleteImpl() {
  stream_.onComplete();
  writerSubscription_.cancel();
}

void FramedWriter::onErrorImpl(folly::exception_wrapper ex) {
  stream_.onError(std::move(ex));
  writerSubscription_.cancel();
}

void FramedWriter::requestImpl(size_t n) {
  writerSubscription_.request(n);
}

void FramedWriter::cancelImpl() {
  writerSubscription_.cancel();
  stream_.onComplete();
}

} // reactive socket
