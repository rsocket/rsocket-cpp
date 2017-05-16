// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBufQueue.h>
#include "src/internal/AllowanceSemaphore.h"
#include "src/internal/ReactiveStreamsCompat.h"
#include "src/temporary_home/SubscriberBase.h"
#include "src/temporary_home/SubscriptionBase.h"

namespace reactivesocket {

struct ProtocolVersion;

class FramedReader : public SubscriberBaseT<std::unique_ptr<folly::IOBuf>>,
                     public SubscriptionBase,
                     public EnableSharedFromThisBase<FramedReader> {
 public:
  explicit FramedReader(
      std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          frames,
      folly::Executor& executor,
      std::shared_ptr<ProtocolVersion> protocolVersion)
      : ExecutorBase(executor),
        frames_(std::move(frames)),
        payloadQueue_(folly::IOBufQueue::cacheChainLength()),
        protocolVersion_(std::move(protocolVersion)) {}

 private:
  // Subscriber methods
  void onSubscribeImpl(
      std::shared_ptr<Subscription> subscription) noexcept override;
  void onNextImpl(std::unique_ptr<folly::IOBuf> element) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper ex) noexcept override;

  // Subscription methods
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  void parseFrames();
  void requestStream();

  bool ensureOrAutodetectProtocolVersion();

  size_t getFrameSizeFieldLength() const;
  size_t getFrameMinimalLength() const;
  size_t getFrameSizeWithLengthField(size_t frameSize) const;
  size_t getPayloadSize(size_t frameSize) const;
  size_t readFrameLength() const;

  using EnableSharedFromThisBase<FramedReader>::shared_from_this;

  std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      frames_;
  std::shared_ptr<Subscription> streamSubscription_;

  AllowanceSemaphore allowance_{0};

  bool streamRequested_{false};
  bool dispatchingFrames_{false};

  folly::IOBufQueue payloadQueue_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};

} // reactivesocket
