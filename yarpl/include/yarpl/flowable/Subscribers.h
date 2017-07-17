// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>
#include <limits>

#include "yarpl/flowable/Subscriber.h"
#include "yarpl/utils/credits.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace flowable {

/// Helper methods for constructing subscriber instances from functions:
/// one, two, or three functions (callables; can be lamda, for instance)
/// may be specified, corresponding to onNext, onError and onSubscribe
/// method bodies in the subscriber.
class Subscribers {
  constexpr static auto kNoFlowControl = credits::kNoFlowControl;

 public:
  template <
      typename T,
      typename Next,
      typename =
          typename std::enable_if<std::is_callable<Next(T), void>::value>::type>
  static auto create(Next&& next, int64_t batch = kNoFlowControl) {
    return Reference<Subscriber<T>>(
        new Base<T, Next>(std::forward<Next>(next), batch));
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(std::exception_ptr), void>::value>::type>
  static auto
  create(Next&& next, Error&& error, int64_t batch = kNoFlowControl) {
    return Reference<Subscriber<T>>(new WithError<T, Next, Error>(
        std::forward<Next>(next), std::forward<Error>(error), batch));
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(std::exception_ptr), void>::value &&
          std::is_callable<Complete(), void>::value>::type>
  static auto create(
      Next&& next,
      Error&& error,
      Complete&& complete,
      int64_t batch = kNoFlowControl) {
    return Reference<Subscriber<T>>(
        new WithErrorAndComplete<T, Next, Error, Complete>(
            std::forward<Next>(next),
            std::forward<Error>(error),
            std::forward<Complete>(complete),
            batch));
  }

 private:
  template <typename T, typename Next>
  class Base : public Subscriber<T> {
   public:
    Base(Next&& next, int64_t batch)
        : next_(std::forward<Next>(next)), batch_(batch), pending_(0) {}

    void onSubscribe(Reference<Subscription> subscription) override {
      Subscriber<T>::onSubscribe(subscription);
      pending_ += batch_;
      subscription->request(batch_);
    }

    void onNext(T value) override {
      next_(std::move(value));
      if (--pending_ < batch_ / 2) {
        const auto delta = batch_ - pending_;
        pending_ += delta;
        Subscriber<T>::subscription()->request(delta);
      }
    }

   private:
    Next next_;
    const int64_t batch_;
    int64_t pending_;
  };

  template <typename T, typename Next, typename Error>
  class WithError : public Base<T, Next> {
   public:
    WithError(Next&& next, Error&& error, int64_t batch)
        : Base<T, Next>(std::forward<Next>(next), batch), error_(error) {}

    void onError(std::exception_ptr error) override {
      Subscriber<T>::onError(error);
      error_(error);
    }

   private:
    Error error_;
  };

  template <typename T, typename Next, typename Error, typename Complete>
  class WithErrorAndComplete : public WithError<T, Next, Error> {
   public:
    WithErrorAndComplete(
        Next&& next,
        Error&& error,
        Complete&& complete,
        int64_t batch)
        : WithError<T, Next, Error>(
              std::forward<Next>(next),
              std::forward<Error>(error),
              batch),
          complete_(complete) {}

    void onComplete() {
      Subscriber<T>::onComplete();
      complete_();
    }

   private:
    Complete complete_;
  };

  Subscribers() = delete;
};

} // flowable
} // yarpl
