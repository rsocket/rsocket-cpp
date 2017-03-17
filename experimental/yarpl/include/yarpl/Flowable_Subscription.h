// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include <tuple>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/flowable/utils/SubscriptionHelper.h"

namespace yarpl {
namespace flowable {

/**
 * Abstract base for creating Subscriptions that
 * - obey the contracts
 * - manage concurrency
 * - delete themselves after onComplete, onError, or cancellation.
 */
template <typename T>
class FlowableSubscription : public reactivestreams_yarpl::Subscription {
 public:
  explicit FlowableSubscription(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> s)
      : subscriber_(std::move(s)) {}
  ~FlowableSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY FlowableSubscription!!!" << std::endl;
  }

  // TODO do we need move semantics?
  FlowableSubscription(FlowableSubscription&&) = delete;
  FlowableSubscription(const FlowableSubscription&) = delete;
  FlowableSubscription& operator=(FlowableSubscription&&) = delete;
  FlowableSubscription& operator=(const FlowableSubscription&) = delete;

  void start() {
    subscriber_->onSubscribe(this);
  }

  void request(int64_t n) override {
    if (n <= 0) {
      return;
    }
    int64_t r = internal::SubscriptionHelper::addCredits(&requested_, n);
    if (r <= 0) {
      return;
    }

    std::tuple<int64_t, bool> emitted = tryEmit();
    int64_t consumed = std::get<0>(emitted);
    bool completed = std::get<1>(emitted);

    if (consumed > 0) {
      internal::SubscriptionHelper::consumeCredits(&requested_, consumed);
    }

    // if we've completed, or been cancelled, delete ourselves
    if (completed) {
      subscriber_->onComplete();
      tryDelete();
      return;
    } else if (internal::SubscriptionHelper::isCancelled(&requested_)) {
      tryDelete();
      return;
    }
  }

  void cancel() override {
    if (internal::SubscriptionHelper::addCancel(&requested_)) {
      // if this is the first time calling cancel, try to delete
      // if this thread wins the lock
      tryDelete();
    }
  }

 protected:
  virtual std::tuple<int64_t, bool> emit(int64_t requested) = 0;

  /**
   * Use inside 'emit' to check for cancellation and break out of a loop.
   * @return
   */
  bool isCancelled() {
    return internal::SubscriptionHelper::isCancelled(&requested_);
  }

  /**
   * Send events to downstream using this from within 'emit'
   * @param t
   */
  void onNext(T& t) {
    subscriber_->onNext(t);
  }

  /**
     * Send events to downstream using this from within 'emit'
     * @param t
     */
  void onNext(T&& t) {
    subscriber_->onNext(std::move(t));
  }

 private:
  /**
   * thread-safe method to try and emit in response to request(n) being called.
   *
   * Only one thread will win.
   *
   * @return int64_t of consumed credits
   * @return bool true if completed, false if not
   * @throws runtime_error if failed
   */
  std::tuple<int64_t, bool> tryEmit() {
    int64_t consumed{0};
    bool isCompleted{false};
    if (emitting_.fetch_add(1) == 0) {
      // this thread won the 0->1 ticket so will execute
      do {
        std::tuple<int64_t, bool> emitted = emit(requested_.load());
        consumed += std::get<0>(emitted);
        isCompleted = std::get<1>(emitted);
        // keep looping until emitting) hits 0 (>1 since fetch_add returns value
        // before decrement)
      } while (emitting_.fetch_add(-1) > 1);
      return std::make_tuple(consumed, isCompleted);
    } else {
      return std::make_tuple(-1, false);
    }
  }

  void tryDelete() {
    // only one thread can call delete
    if (emitting_.fetch_add(1) == 0) {
      // TODO remove this cout once happy with it
      std::cout << "Delete FlowableSubscription" << std::endl;
      delete this;
    }
  }

  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> subscriber_;
  std::atomic_ushort emitting_{0};
  std::atomic<std::int64_t> requested_{0};
};
}
}
