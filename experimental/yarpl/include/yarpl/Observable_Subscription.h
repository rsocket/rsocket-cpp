// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <memory>

namespace yarpl {
namespace observable {

// forward declaration of Observer since it's a cycle
template <typename T>
class Observer;
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
  std::atomic_bool cancelled_{false};
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

/**
 * Abstract base for creating Subscriptions that
 * - obey the contracts
 * - manage concurrency
 * - delete themselves after onComplete, onError, or cancellation.
 */
template <typename T>
class ObservableSubscription : public Subscription {
 public:
  explicit ObservableSubscription(std::unique_ptr<Observer<T>> o)
      : observer_(std::move(o)) {
    observer_->onSubscribe(this);
  }
  ~ObservableSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY ObservableSubscription!!!" << std::endl;
  }

  // TODO do we need move semantics?
  ObservableSubscription(ObservableSubscription&&) = delete;
  ObservableSubscription(const ObservableSubscription&) = delete;
  ObservableSubscription& operator=(ObservableSubscription&&) = delete;
  ObservableSubscription& operator=(const ObservableSubscription&) = delete;

  void cancel() override {
    bool expected = false;
    if (cancelled_.compare_exchange_strong(expected, true)) {
      // if this is the first time calling cancel, try to delete
      // if this thread wins the lock
      tryDelete();
    }
  }

  bool isCancelled() const override {
    return cancelled_;
  }

  virtual void start() = 0;

  /**
   *Send events to downstream using this from within 'emit'
   *@param t
   */
  void onNext(const T& t) {
    observer_->onNext(t);
  }

  /**
     * Send events to downstream using this from within 'emit'
     * @param t
     */
  void onNext(T&& t) {
    observer_->onNext(std::move(t));
  }

  void onComplete() {
    if (~terminated_ && ~cancelled_) {
      observer_->onComplete();
      tryDelete();
    }
    tryDelete();
  }

  void onError(const std::exception_ptr error) {
    if (~terminated_ && ~cancelled_) {
      observer_->onError(error);
      tryDelete();
    }
  }

 private:
  void tryDelete() {
    bool expected = false;
    // only one invocation
    if (terminated_.compare_exchange_strong(expected, true)) {
      // TODO remove this cout once happy with it
      std::cout << "Delete ObservableSubscription" << std::endl;
      delete this;
    }
  }

  std::unique_ptr<Observer<T>> observer_;
  // TODO is it more memory efficient to use an atomic<short>
  std::atomic_bool cancelled_{false};
  std::atomic_bool terminated_{false};
};
} // observable namespace
} // yarpl namespace
