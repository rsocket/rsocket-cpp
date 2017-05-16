// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/observable/Subscriptions.h"
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

Reference<Subscription> Subscriptions::create(std::function<void()> onCancel) {
  return Reference<Subscription>(new CallbackSubscription(std::move(onCancel)));
}

Reference<Subscription> Subscriptions::create(std::atomic_bool& cancelled) {
  return create([&cancelled]() { cancelled = true; });
}

Reference<Subscription> Subscriptions::empty() {
  return Reference<Subscription>(new AtomicBoolSubscription());
}

Reference<AtomicBoolSubscription> Subscriptions::atomicBoolSubscription() {
  return Reference<AtomicBoolSubscription>(new AtomicBoolSubscription());
}
}
}
