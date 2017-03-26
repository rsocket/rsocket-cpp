// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/Observable_Subscription.h"
#include <atomic>
#include <iostream>

namespace yarpl {
namespace observable {

/**
 * Implementation that allows checking if a Subscription is cancelled.
 */
void AtomicBoolSubscription::cancel() {
  cancelled_ = true;
}

bool AtomicBoolSubscription::isCancelled() const {
  return cancelled_;
}

/**
 * Implementation that gets a callback when cancellation occurs.
 */
CallbackSubscription::CallbackSubscription(std::function<void()>&& onCancel)
    : onCancel_(std::move(onCancel)) {}

void CallbackSubscription::cancel() {
  bool expected = false;
  // mark cancelled 'true' and only if successful invoke 'onCancel()'
  if (cancelled_.compare_exchange_strong(expected, true)) {
    onCancel_();
  }
}

bool CallbackSubscription::isCancelled() const {
  return cancelled_;
}

std::unique_ptr<Subscription> Subscriptions::create(
    std::function<void()> onCancel) {
  return std::make_unique<CallbackSubscription>(std::move(onCancel));
}

std::unique_ptr<Subscription> Subscriptions::create(
    std::atomic_bool& cancelled) {
  return create([&cancelled]() { cancelled = true; });
}

std::unique_ptr<Subscription> Subscriptions::create() {
  return std::make_unique<AtomicBoolSubscription>();
}
}
}
