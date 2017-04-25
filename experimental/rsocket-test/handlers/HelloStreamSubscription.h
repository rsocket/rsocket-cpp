// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <string>
#include "src/DuplexConnection.h"
#include "src/Payload.h"
#include "src/SubscriptionBase.h"

namespace rsocket {
namespace tests {

/// Emits a stream of ints
class HelloStreamSubscription : public reactivesocket::SubscriptionBase {
 public:
  explicit HelloStreamSubscription(
      std::shared_ptr<reactivesocket::Subscriber<reactivesocket::Payload>>
          subscriber,
      std::string name,
      size_t numberToEmit = 2)
      : ExecutorBase(reactivesocket::defaultExecutor()),
        subscriber_(std::move(subscriber)),
        name_(std::move(name)),
        numberToEmit_(numberToEmit),
        cancelled_(false) {}

 private:
  // Subscription methods
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  std::shared_ptr<reactivesocket::Subscriber<reactivesocket::Payload>>
      subscriber_;
  std::string name_;
  size_t numberToEmit_;
  size_t currentElem_ = 0;
  std::atomic_bool cancelled_;
};
}
}
