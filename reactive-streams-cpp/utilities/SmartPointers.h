// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <utility>

namespace reactivestreams {

/// A "smart pointer" to an arbitrary Subscriber.
///
/// Accessing a Subscriber via this class only ensures that a terminal signal is
/// delivered to the pointee exactly once. Note that Subscriber::onSubscriber
/// must be delivered to the pointee before it is wrapped in SubscriberPtr.
///
/// This class is not thread-safe. User must provide external synchronisation.
template <typename S>
class SubscriberPtr {
 public:
  SubscriberPtr() = default;
  explicit SubscriberPtr(S* subscriber) : subscriber_(subscriber) {
    assert(subscriber_);
  }

  SubscriberPtr(const SubscriberPtr& rhs) = delete;
  SubscriberPtr& operator=(const SubscriberPtr& rhs) = delete;

  SubscriberPtr(SubscriberPtr&& rhs) noexcept : subscriber_(rhs.release()) {}
  SubscriberPtr& operator=(SubscriberPtr&& rhs) noexcept {
    subscriber_ = rhs.release();
  }

  ~SubscriberPtr() noexcept {
    // Tail-call
    reset();
  }

  void reset(S* subscriber = nullptr) {
    auto* old_subscriber = subscriber_;
    subscriber_ = subscriber;
    if (old_subscriber) {
      // Tail-call
      old_subscriber->onComplete();
    }
  }

  S* release() {
    auto* subscriber = subscriber_;
    subscriber_ = nullptr;
    return subscriber;
  }

  S* get() const {
    return subscriber_;
  }

  explicit operator bool() const {
    return subscriber_;
  }

  void onNext(typename S::ElementType element) const {
    // Tail-call
    assert(subscriber_);
    subscriber_->onNext(std::move(element));
  }

  void onComplete() {
    if (auto* subscriber = release()) {
      // Tail-call
      subscriber->onComplete();
    }
  }

  void onError(typename S::ErrorType ex) {
    if (auto* subscriber = release()) {
      // Tail-call
      subscriber->onError(std::move(ex));
    }
  }

 private:
  S* subscriber_{nullptr};
};

template <typename S>
SubscriberPtr<S> makeSubscriberPtr(S* subscriber) {
  return SubscriberPtr<S>(subscriber);
}

/// A "smart pointer" to an arbitrary Subscription.
///
/// Accessing a Subscription via this class only ensures that a terminal signal
/// is delivered to the pointee exactly once.
///
/// This class is not thread-safe. User must provide external synchronisation.
template <typename S>
class SubscriptionPtr {
 public:
  SubscriptionPtr() = default;
  explicit SubscriptionPtr(S* subscription) : subscription_(subscription) {
    assert(subscription_);
  }

  SubscriptionPtr(const SubscriptionPtr& rhs) = delete;
  SubscriptionPtr& operator=(const SubscriptionPtr& rhs) = delete;

  SubscriptionPtr(SubscriptionPtr&& rhs) noexcept
      : subscription_(rhs.release()) {}
  SubscriptionPtr& operator=(SubscriptionPtr&& rhs) noexcept {
    subscription_ = rhs.release();
  }

  ~SubscriptionPtr() noexcept {
    // Tail-call
    reset();
  }

  void reset(S* subscription = nullptr) {
    auto* old_subscription = subscription_;
    subscription_ = subscription;
    if (old_subscription) {
      // Tail-call
      old_subscription->cancel();
    }
  }

  S* release() {
    auto* subscription = subscription_;
    subscription_ = nullptr;
    return subscription;
  }

  S* get() const {
    return subscription_;
  }

  explicit operator bool() const {
    return subscription_;
  }

  void request(size_t n) const {
    // Tail-call
    assert(subscription_);
    subscription_->request(n);
  }

  void cancel() {
    if (auto* subscription = release()) {
      // Tail-call
      subscription->cancel();
    }
  }

 private:
  S* subscription_{nullptr};
};

template <typename S>
SubscriptionPtr<S> makeSubscriptionPtr(S* subscription) {
  return SubscriptionPtr<S>(subscription);
}
}
