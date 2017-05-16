// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <vector>
#include "src/internal/ReactiveStreamsCompat.h"
#include "src/temporary_home/SubscriberBase.h"
#include "src/temporary_home/SubscriptionBase.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

struct ProtocolVersion;

class FramedWriter : public SubscriberBaseT<std::unique_ptr<folly::IOBuf>>,
                     public SubscriptionBase,
                     public EnableSharedFromThisBase<FramedWriter> {
 public:
  explicit FramedWriter(
      std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          stream,
      folly::Executor& executor,
      std::shared_ptr<ProtocolVersion> protocolVersion)
      : ExecutorBase(executor),
        stream_(std::move(stream)),
        protocolVersion_(std::move(protocolVersion)) {}

  void onNextMultiple(std::vector<std::unique_ptr<folly::IOBuf>> element);

 private:
  // Subscriber methods
  void onSubscribeImpl(std::shared_ptr<reactivesocket::Subscription>
                           subscription) noexcept override;
  void onNextImpl(std::unique_ptr<folly::IOBuf> element) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper ex) noexcept override;

  // Subscription methods
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  size_t getFrameSizeFieldLength() const;
  size_t getPayloadLength(size_t payloadLength) const;

  std::unique_ptr<folly::IOBuf> appendSize(
      std::unique_ptr<folly::IOBuf> payload);

  using EnableSharedFromThisBase<FramedWriter>::shared_from_this;

  std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      stream_;
  std::shared_ptr<::reactivestreams::Subscription> writerSubscription_;
  std::shared_ptr<ProtocolVersion> protocolVersion_;
};

} // reactivesocket
