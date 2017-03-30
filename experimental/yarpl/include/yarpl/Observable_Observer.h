// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Observable_Subscription.h"

namespace yarpl {
namespace observable {

/**
 * Passed as unique_ptr to Observable.subscribe which SHOULD std::move it into a
 * Subscription which owns its lifecycle.
 *
 * @tparam T
 */
template <typename T>
class Observer {
 public:
  virtual ~Observer() = default;

  /**
   * Receive the Subscription that controls the lifecycle of this
   * Observer/Subscription pair.
   *
   * This Subscription* will exist until onComplete, onError, or cancellation.
   *
   * Rules:
   * - Do NOT use Subscription* after onComplete();
   * - Do NOT use Subscription* after onError();
   * - Do NOT use Subscription* after calling Subscription*->cancel();
   */
  virtual void onSubscribe(yarpl::observable::Subscription*) = 0;
  virtual void onNext(const T&) = 0;
  virtual void onNext(T&&) = 0;
  virtual void onComplete() = 0;
  virtual void onError(const std::exception_ptr error) = 0;
};

static const std::function<void(const std::exception_ptr)> kDefaultError =
    [](const std::exception_ptr) {};
static const std::function<void()> kDefaultComplete = []() {};

class Observers {
 public:
  template <typename T>
  static std::unique_ptr<Observer<T>> create(
      std::function<void(const T&)> onNext) {
    return std::unique_ptr<Observer<T>>(new ObserverImpl<T>(onNext));
  }

  template <typename T>
  static std::unique_ptr<Observer<T>> create(
      std::function<void(const T&)> onNext,
      std::function<void(const std::exception_ptr)> onError) {
    return std::unique_ptr<Observer<T>>(new ObserverImpl<T>(onNext, onError));
  }

  template <typename T>
  static std::unique_ptr<Observer<T>> create(
      std::function<void(const T&)> onNext,
      std::function<void(const std::exception_ptr)> onError,
      std::function<void()> onComplete) {
    return std::unique_ptr<Observer<T>>(
        new ObserverImpl<T>(onNext, onError, onComplete));
  }

 private:
  template <typename T>
  class ObserverImpl : public Observer<T> {
   public:
    explicit ObserverImpl(std::function<void(const T&)> n)
        : next_(std::move(n)),
          error_(kDefaultError),
          complete_(kDefaultComplete) {}
    ObserverImpl(
        std::function<void(const T&)> n,
        std::function<void(const std::exception_ptr)> e)
        : next_(std::move(n)),
          error_(std::move(e)),
          complete_(kDefaultComplete) {}
    ObserverImpl(
        std::function<void(const T&)> n,
        std::function<void(const std::exception_ptr)> e,
        std::function<void()> c)
        : next_(std::move(n)), error_(std::move(e)), complete_(std::move(c)) {}

    void onNext(const T& value) override {
      next_(value);
    }

    void onNext(T&& value) override {
      next_(value);
    }

    void onError(const std::exception_ptr e) override {
      error_(e);
    }

    void onComplete() override {
      complete_();
    }

    void onSubscribe(Subscription* s) override {
      // do nothing with onSubscribe when lambdas used
      // cancellation is not supported in that case
    }

   private:
    std::function<void(const T&)> next_;
    std::function<void(const T&&)> nextMove_;
    std::function<void(const std::exception_ptr)> error_;
    std::function<void()> complete_;
  };
};

} // observable namespace
} // yarpl namespace
