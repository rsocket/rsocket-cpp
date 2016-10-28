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
    runInExecutor(
        std::bind(&SubscriptionBase::requestImpl, shared_from_this(), n));
  }

  void cancel() override final {
    runInExecutor(std::bind(&SubscriptionBase::cancelImpl, shared_from_this()));
  }
};

} // reactivesocket
