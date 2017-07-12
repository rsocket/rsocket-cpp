// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBufQueue.h>
#include "src/internal/AllowanceSemaphore.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

struct ProtocolVersion;

class FramedReader : public yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>,
                     public yarpl::flowable::Subscription {
 using SubscriberBase = yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>;

 public:
  explicit FramedReader(std::shared_ptr<ProtocolVersion> protocolVersion)
      : payloadQueue_(folly::IOBufQueue::cacheChainLength()),
        protocolVersion_(std::move(protocolVersion)) {}

  void setInput(yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>>
                frames);

 private:
  // Subscriber methods
  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept override;
  void onNext(std::unique_ptr<folly::IOBuf> element) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr ex) noexcept override;

  // Subscription methods
  void request(int64_t n) noexcept override;
  void cancel() noexcept override;

  void error(std::string errorMsg);
  void parseFrames();
  bool ensureOrAutodetectProtocolVersion();

  size_t getFrameSizeFieldLength() const;
  size_t getFrameMinimalLength() const;
  size_t getFrameSizeWithLengthField(size_t frameSize) const;
  size_t getPayloadSize(size_t frameSize) const;
  size_t readFrameLength() const;

  yarpl::Reference<yarpl::flowable::Subscriber<std::unique_ptr<folly::IOBuf>>> frames_;

  AllowanceSemaphore allowance_{0};

  bool completed_{false};
  bool dispatchingFrames_{false};

  folly::IOBufQueue payloadQueue_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};

} // reactivesocket
