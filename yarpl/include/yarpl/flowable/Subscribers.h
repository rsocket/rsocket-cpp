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
  static Reference<Subscriber<T>> create(
      Next next,
      int64_t batch = kNoFlowControl) {
    return make_ref<Base<T, Next>>(std::move(next), batch);
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(folly::exception_wrapper), void>::value>::type>
  static Reference<Subscriber<T>>
  create(Next next, Error error, int64_t batch = kNoFlowControl) {
    return make_ref<WithError<T, Next, Error>>(
        std::move(next), std::move(error), batch);
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(folly::exception_wrapper), void>::value &&
          std::is_callable<Complete(), void>::value>::type>
  static Reference<Subscriber<T>> create(
      Next next,
      Error error,
      Complete complete,
      int64_t batch = kNoFlowControl) {
    return make_ref<WithErrorAndComplete<T, Next, Error, Complete>>(
        std::move(next), std::move(error), std::move(complete), batch);
  }

 private:
  template <typename T, typename Next>
  class Base : public Subscriber<T> {
   public:
    Base(Next next, int64_t batch)
        : next_(std::move(next)), batch_(batch), pending_(0) {}

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
    WithError(Next next, Error error, int64_t batch)
        : Base<T, Next>(std::move(next), batch), error_(std::move(error)) {}

    void onError(folly::exception_wrapper error) override {
      Subscriber<T>::onError(error);
      error_(std::move(error));
    }

   private:
    Error error_;
  };

  template <typename T, typename Next, typename Error, typename Complete>
  class WithErrorAndComplete : public WithError<T, Next, Error> {
   public:
    WithErrorAndComplete(
        Next next,
        Error error,
        Complete complete,
        int64_t batch)
        : WithError<T, Next, Error>(
              std::move(next),
              std::move(error),
              batch),
          complete_(std::move(complete)) {}

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
