// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/single/SingleSubscriptions.h"
#include <atomic>
#include <iostream>

namespace yarpl {
namespace single {

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

Reference<SingleSubscription> SingleSubscriptions::create(std::function<void()> onCancel) {
  return Reference<SingleSubscription>(new CallbackSubscription(std::move(onCancel)));
}

Reference<SingleSubscription> SingleSubscriptions::create(std::atomic_bool& cancelled) {
  return create([&cancelled]() { cancelled = true; });
}

Reference<SingleSubscription> SingleSubscriptions::empty() {
  return Reference<SingleSubscription>(new AtomicBoolSubscription());
}

Reference<AtomicBoolSubscription> SingleSubscriptions::atomicBoolSubscription() {
  return Reference<AtomicBoolSubscription>(new AtomicBoolSubscription());
}
} // single
} // yarpl
