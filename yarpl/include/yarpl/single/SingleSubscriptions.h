// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <functional>

#include "../Refcounted.h"
#include "SingleSubscription.h"

namespace yarpl {
namespace single {

/**
* Implementation that allows checking if a Subscription is cancelled.
*/
class AtomicBoolSubscription : public SingleSubscription {
 public:
  void cancel() override;
  bool isCancelled() const;

 private:
  std::atomic_bool cancelled_{false};
};

/**
* Implementation that gets a callback when cancellation occurs.
*/
class CallbackSubscription : public SingleSubscription {
 public:
  explicit CallbackSubscription(std::function<void()>&& onCancel);
  void cancel() override;
  bool isCancelled() const;

 private:
  std::atomic_bool cancelled_{false};
  std::function<void()> onCancel_;
};

class SingleSubscriptions {
 public:
  static Reference<SingleSubscription> create(std::function<void()> onCancel);
  static Reference<SingleSubscription> create(std::atomic_bool& cancelled);
  static Reference<SingleSubscription> empty();
  static Reference<AtomicBoolSubscription> atomicBoolSubscription();
};

} // single namespace
} // yarpl namespace
