// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace yarpl {
namespace observable {

/**
 * Emitted from Observable.subscribe to Observer.onSubscribe.
 * Implementations of this SHOULD own the Subscriber lifecycle.
 */
class Subscription {
 public:
  virtual ~Subscription() = default;
  virtual void cancel() = 0;
  virtual bool isCancelled() const = 0;
};

/**
* Implementation that allows checking if a Subscription is cancelled.
*/
class AtomicBoolSubscription : public Subscription {
 public:
  void cancel() override;
  bool isCancelled() const override;

 private:
  std::atomic_bool canceled_{false};
};

/**
* Implementation that gets a callback when cancellation occurs.
*/
class CallbackSubscription : public Subscription {
 public:
  explicit CallbackSubscription(std::function<void()>&& onCancel);
  void cancel() override;
  bool isCancelled() const override;

 private:
  std::atomic_bool cancelled_{false};
  std::function<void()> onCancel_;
};

class Subscriptions {
 public:
  static std::unique_ptr<Subscription> create(std::function<void()> onCancel);
  static std::unique_ptr<Subscription> create(std::atomic_bool& cancelled);
  static std::unique_ptr<Subscription> create();
};
} // observable namespace
} // yarpl namespace
