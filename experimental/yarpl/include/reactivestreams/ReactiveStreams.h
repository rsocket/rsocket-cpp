// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>
#include <memory>

namespace reactivestreams_yarpl {
// TODO remove 'yarpl' if we agree on this usage elsewhere

/**
 * Emitted from Publisher.subscribe to Subscriber.onSubscribe.
 * Implementations of this SHOULD own the Subscriber lifecycle.
 */
class Subscription {
 public:
  virtual ~Subscription() = default;
  virtual void request(int64_t n) = 0;
  virtual void cancel() = 0;
};

/**
 * Passed as unique_ptr to Flowable.subscribe which SHOULD std::move it into a
 * Subscription which owns its lifecycle.
 *
 * @tparam T
 */
template <typename T>
class Subscriber {
 public:
  virtual ~Subscriber() = default;

  /**
   * Receive the Subscription that controls the lifecycle of this
   * Subscriber/Subscription pair.
   *
   * This Subscription* will exist until onComplete, onError, or cancellation.
   *
   * Rules:
   * - Do NOT use Subscription* after onComplete();
   * - Do NOT use Subscription* after onError();
   * - Do NOT use Subscription* after calling Subscription*->cancel();
   */
  virtual void onSubscribe(Subscription*) = 0;
  virtual void onNext(const T&) = 0;
  virtual void onComplete() = 0;
  virtual void onError(const std::exception_ptr error) = 0;
};

template <typename T>
class Publisher {
 public:
  virtual ~Publisher() = default;
  virtual void subscribe(std::unique_ptr<Subscriber<T>>) = 0;
};
}
