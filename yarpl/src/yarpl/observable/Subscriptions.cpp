// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/observable/Subscriptions.h"
#include <atomic>
#include <iostream>
#include <glog/logging.h>

namespace yarpl {
namespace observable {

/**
 * Implementation that allows checking if a Subscription is cancelled.
 */
void Subscription::cancel() {
  cancelled_ = true;
  // Lock must be obtained here and not in the range expression for it to
  // apply to the loop body.
  auto locked = tiedSubscriptions_.wlock();
  for(auto& subscription : *locked) {
    subscription->cancel();
  }
}

bool Subscription::isCancelled() const {
  return cancelled_;
}

void Subscription::tieSubscription(Reference<Subscription> subscription) {
  CHECK(subscription);
  if (isCancelled()) {
    subscription->cancel();
  }
  tiedSubscriptions_->push_back(std::move(subscription));
}

/**
 * Implementation that gets a callback when cancellation occurs.
 */
CallbackSubscription::CallbackSubscription(std::function<void()> onCancel)
    : onCancel_(std::move(onCancel)) {}

void CallbackSubscription::cancel() {
  bool expected = false;
  // mark cancelled 'true' and only if successful invoke 'onCancel()'
  if (cancelled_.compare_exchange_strong(expected, true)) {
    onCancel_();
    // Lock must be obtained here and not in the range expression for it to
    // apply to the loop body.
    auto locked = tiedSubscriptions_.wlock();
    for(auto& subscription : *locked) {
      subscription->cancel();
    }
  }
}

Reference<Subscription> Subscriptions::create(std::function<void()> onCancel) {
  return make_ref<CallbackSubscription>(std::move(onCancel));
}

Reference<Subscription> Subscriptions::create(std::atomic_bool& cancelled) {
  return create([&cancelled]() { cancelled = true; });
}

Reference<Subscription> Subscriptions::create() {
  return make_ref<Subscription>();
}

}
}
