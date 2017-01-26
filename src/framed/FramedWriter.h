// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <vector>
#include "src/ReactiveStreamsCompat.h"
#include "src/SmartPointers.h"
#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"

namespace folly {
class IOBuf;
}

namespace reactivesocket {

class FramedWriter : public SubscriberBaseT<std::unique_ptr<folly::IOBuf>>,
                     public SubscriptionBase,
                     public EnableSharedFromThisBase<FramedWriter> {
 public:
  explicit FramedWriter(
      std::shared_ptr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
          stream)
      : ExecutorBase(defaultExecutor()), stream_(std::move(stream)) {}

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

  using EnableSharedFromThisBase<FramedWriter>::shared_from_this;

  SubscriberPtr<reactivesocket::Subscriber<std::unique_ptr<folly::IOBuf>>>
      stream_;
  SubscriptionPtr<::reactivestreams::Subscription> writerSubscription_;
};

} // reactivesocket
