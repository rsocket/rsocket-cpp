// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/MoveWrapper.h>
#include "src/EnableSharedFromThis.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ExecutorMixin.h"

namespace reactivesocket {

class SubscriptionBase : public Subscription,
                         public EnableSharedFromThisBase<SubscriptionBase>,
                         public virtual ExecutorBase {
  virtual void requestImpl(size_t n) = 0;
  virtual void cancelImpl() = 0;

 public:
  using ExecutorBase::ExecutorBase;

  void request(size_t n) override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, n]() {
      VLOG(1) << (ExecutorBase*)thisPtr.get() << " request";
      thisPtr->requestImpl(n);
    });
  }

  void cancel() override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr]() {
      VLOG(1) << (ExecutorBase*)thisPtr.get() << " cancel";
      thisPtr->cancelImpl();
    });
  }
};

} // reactivesocket
