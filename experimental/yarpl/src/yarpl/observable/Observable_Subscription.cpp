// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/Observable_Subscription.h"

namespace yarpl {
namespace observable {

std::unique_ptr<Subscription> Subscription::create(
    std::function<void()> onCancel) {
  return std::make_unique<Subscription>(std::move(onCancel));
}

std::unique_ptr<Subscription> Subscription::create(
    std::atomic_bool& cancelled) {
  return create([&cancelled]() { cancelled = true; });
}

std::unique_ptr<Subscription> Subscription::create() {
  return create([]() {});
}

void Subscription::cancel() {
  bool expected = false;
  // mark cancelled 'true' and only if successful invoke 'onCancel()'
  if (cancelled_.compare_exchange_strong(expected, true)) {
    onCancel_();
  }
}

bool Subscription::isCanceled() const {
  return cancelled_;
}
}
}
