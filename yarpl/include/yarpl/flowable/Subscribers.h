// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>
#include <limits>

#include <folly/functional/Invoke.h>

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
          typename std::enable_if<folly::is_invocable<Next, T>::value>::type>
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
          folly::is_invocable<Next, T>::value &&
          folly::is_invocable<Error, folly::exception_wrapper>::value>::type>
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
          folly::is_invocable<Next, T>::value &&
          folly::is_invocable<Error, folly::exception_wrapper>::value &&
          folly::is_invocable<Complete>::value>::type>
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
  class Base : public BaseSubscriber<T> {
   public:
    Base(Next next, int64_t batch)
        : next_(std::move(next)), batch_(batch), pending_(0) {}

    void onSubscribeImpl() override final {
      pending_ += batch_;
      this->request(batch_);
    }

    void onNextImpl(T value) override final {
      try {
        next_(std::move(value));
      } catch (const std::exception& exn) {
        this->cancel();
        auto ew = folly::exception_wrapper{std::current_exception(), exn};
        LOG(ERROR) << "'next' method should not throw: " << ew.what();
        onErrorImpl(ew);
        return;
      }

      if (--pending_ < batch_ / 2) {
        const auto delta = batch_ - pending_;
        pending_ += delta;
        this->request(delta);
      }
    }

    void onCompleteImpl() override {}
    void onErrorImpl(folly::exception_wrapper) override {}

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

    void onErrorImpl(folly::exception_wrapper error) override final {
      try {
        error_(std::move(error));
      } catch (const std::exception& exn) {
        auto ew = folly::exception_wrapper{std::current_exception(), exn};
        LOG(ERROR) << "'error' method should not throw: " << ew.what();
#ifndef NDEBUG
        throw ew; // Throw the wrapped exception
#endif
      }
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
        : WithError<T, Next, Error>(std::move(next), std::move(error), batch),
          complete_(std::move(complete)) {}

    void onCompleteImpl() override final {
      try {
        complete_();
      } catch (const std::exception& exn) {
        auto ew = folly::exception_wrapper{std::current_exception(), exn};
        LOG(ERROR) << "'complete' method should not throw: " << ew.what();
#ifndef NDEBUG
        throw ew; // Throw the wrapped exception
#endif
      }
    }

   private:
    Complete complete_;
  };

  Subscribers() = delete;
};

} // namespace flowable
} // namespace yarpl
