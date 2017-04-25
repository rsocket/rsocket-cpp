// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/EnableSharedFromThis.h"
#include "src/Executor.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

class SubscriptionBase : public Subscription,
                         public EnableSharedFromThisBase<SubscriptionBase>,
                         public virtual ExecutorBase {
  virtual void requestImpl(size_t n) noexcept = 0;
  virtual void cancelImpl() noexcept = 0;

 public:
  // initialization of the ExecutorBase will be ignored for any of the
  // classes deriving from SubscriptionBase
  // providing the default param values just to make the compiler happy
  explicit SubscriptionBase(folly::Executor& executor = defaultExecutor())
      : ExecutorBase(executor) {}

  void request(size_t n) noexcept override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, n]() {
      thisPtr->requestImpl(n);
    });
  }

  void cancel() noexcept override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr]() {
      thisPtr->cancelImpl();
    });
  }
};

} // reactivesocket
