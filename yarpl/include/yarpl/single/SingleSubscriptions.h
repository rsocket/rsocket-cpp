// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <functional>
#include <mutex>

#include "yarpl/Refcounted.h"
#include "yarpl/single/SingleSubscription.h"

namespace yarpl {
namespace single {

/**
* Implementation that allows checking if a Subscription is cancelled.
*/
class AtomicBoolSingleSubscription : public SingleSubscription {
 public:
  void cancel() override {
    cancelled_ = true;
  }
  bool isCancelled() const {
    return cancelled_;
  }

 private:
  std::atomic_bool cancelled_{false};
};

/**
* Implementation that gets a callback when cancellation occurs.
*/
class CallbackSingleSubscription : public SingleSubscription {
 public:
  explicit CallbackSingleSubscription(std::function<void()>&& onCancel)
      : onCancel_(std::move(onCancel)) {}
  void cancel() override {
    bool expected = false;
    // mark cancelled 'true' and only if successful invoke 'onCancel()'
    if (cancelled_.compare_exchange_strong(expected, true)) {
      onCancel_();
    }
  }
  bool isCancelled() const {
    return cancelled_;
  }

 private:
  std::atomic_bool cancelled_{false};
  std::function<void()> onCancel_;
};

/**
* Implementation that can be cancelled with or without
 * a delegate, and when the delegate exists (before or after cancel)
 * it will be cancelled in a thread-safe manner.
*/
class DelegateSingleSubscription : public SingleSubscription {
 public:
  explicit DelegateSingleSubscription() {}
  void cancel() override {
    bool shouldCancelDelegate = false;
    {
      std::lock_guard<std::mutex> g(m_);
      cancelled_ = true;
      if (delegate_) {
        shouldCancelDelegate = true;
      }
    }
    // cancel without holding lock
    if (shouldCancelDelegate) {
      delegate_->cancel();
    }
  }
  bool isCancelled() const {
    std::lock_guard<std::mutex> g(m_);
    return cancelled_;
  }
  /**
   * This can be called once.
   */
  void setDelegate(Reference<SingleSubscription> d) {
    bool shouldCancelDelegate = false;
    {
      std::lock_guard<std::mutex> g(m_);
      if (delegate_) {
        throw std::runtime_error("Delegate already set. Only one permitted.");
      }
      delegate_ = std::move(d);
      if (cancelled_) {
        shouldCancelDelegate = true;
      }
    }
    // cancel without holding lock
    if (shouldCancelDelegate) {
      delegate_->cancel();
    }
  }

 private:
  // all must be protected by a mutex
  mutable std::mutex m_;
  bool cancelled_{false};
  Reference<SingleSubscription> delegate_;
};

class SingleSubscriptions {
 public:
  static Reference<CallbackSingleSubscription> create(
      std::function<void()> onCancel) {
    return make_ref<CallbackSingleSubscription>(std::move(onCancel));
  }
  static Reference<CallbackSingleSubscription> create(
      std::atomic_bool& cancelled) {
    return create([&cancelled]() { cancelled = true; });
  }
  static Reference<SingleSubscription> empty() {
    return Reference<SingleSubscription>(new AtomicBoolSingleSubscription());
  }
  static Reference<AtomicBoolSingleSubscription> atomicBoolSubscription() {
    return make_ref<AtomicBoolSingleSubscription>();
  }
};

} // single namespace
} // yarpl namespace
