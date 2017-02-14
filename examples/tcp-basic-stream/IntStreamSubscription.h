// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/DuplexConnection.h"
#include "src/Payload.h"
#include "src/SubscriptionBase.h"

namespace reactivesocket {

/// Emits a stream of ints
class IntStreamSubscription : public SubscriptionBase {
 public:
  explicit IntStreamSubscription(
      std::shared_ptr<Subscriber<Payload>> subscriber,
      size_t numberToEmit = 2)
      : ExecutorBase(defaultExecutor()),
        subscriber_(std::move(subscriber)),
        numberToEmit_(numberToEmit),
        cancelled(false) {}

 private:
  // Subscription methods
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  std::shared_ptr<Subscriber<Payload>> subscriber_;
  size_t numberToEmit_;
  size_t currentElem_ = 0;
  std::atomic_bool cancelled;
};
}