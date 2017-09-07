// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBufQueue.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/internal/AllowanceSemaphore.h"
#include "rsocket/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

class FramedReader : public DuplexConnection::Subscriber,
                     public yarpl::flowable::Subscription {
 public:
  explicit FramedReader(std::shared_ptr<ProtocolVersion> version)
      : version_{std::move(version)} {}

  void setInput(yarpl::Reference<DuplexConnection::Subscriber>);

  // Subscriber.

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>) override;
  void onNext(std::unique_ptr<folly::IOBuf>) override;
  void onComplete() override;
  void onError(folly::exception_wrapper) override;

  // Subscription.

  void request(int64_t) override;
  void cancel() override;

 private:
  void error(std::string errorMsg);
  void parseFrames();
  bool ensureOrAutodetectProtocolVersion();

  size_t readFrameLength() const;

  yarpl::Reference<DuplexConnection::Subscriber> inner_;

  AllowanceSemaphore allowance_;
  bool dispatchingFrames_{false};

  folly::IOBufQueue payloadQueue_{folly::IOBufQueue::cacheChainLength()};
  std::shared_ptr<ProtocolVersion> version_;
};
}
