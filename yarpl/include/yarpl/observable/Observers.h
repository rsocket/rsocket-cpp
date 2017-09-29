// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <exception>
#include <limits>
#include <folly/ExceptionWrapper.h>

#include "yarpl/observable/Observer.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace observable {

/// Helper methods for constructing subscriber instances from functions:
/// one, two, or three functions (callables; can be lamda, for instance)
/// may be specified, corresponding to onNext, onError and onComplete
/// method bodies in the subscriber.
class Observers {
 private:
  /// Defined if Next, Error and Complete are signature-compatible with
  /// onNext, onError and onComplete subscriber methods respectively.
  template <
      typename T,
      typename Next,
      typename Error = void (*)(folly::exception_wrapper),
      typename Complete = void (*)()>
  using EnableIfCompatible = typename std::enable_if<
      std::is_callable<Next(T), void>::value &&
      std::is_callable<Error(folly::exception_wrapper), void>::value &&
      std::is_callable<Complete(), void>::value>::type;

 public:
  template <typename T, typename Next, typename = EnableIfCompatible<T, Next>>
  static auto create(Next next) {
    return make_ref<Base<T, Next>, Observer<T>>(std::move(next));
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename = EnableIfCompatible<T, Next, Error>>
  static auto create(Next next, Error error) {
    return make_ref<WithError<T, Next, Error>, Observer<T>>(
        std::move(next), std::move(error));
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename Complete,
      typename = EnableIfCompatible<T, Next, Error, Complete>>
  static auto create(Next next, Error error, Complete complete) {
    return make_ref<
        WithErrorAndComplete<T, Next, Error, Complete>,
        Observer<T>>(std::move(next), std::move(error), std::move(complete));
  }

  template <typename T>
  static auto createNull() {
    return make_ref<NullObserver<T>>();
  }

 private:
  template <typename T>
  class NullObserver : public Observer<T> {
   public:
    void onNext(T) {}
  };

  template <typename T, typename Next>
  class Base : public Observer<T> {
   public:
    explicit Base(Next next) : next_(std::move(next)) {}

    void onNext(T value) override {
      next_(std::move(value));
    }

   private:
    Next next_;
  };

  template <typename T, typename Next, typename Error>
  class WithError : public Base<T, Next> {
   public:
    WithError(Next next, Error error)
        : Base<T, Next>(std::move(next)), error_(std::move(error)) {}

    void onError(folly::exception_wrapper error) override {
      error_(std::move(error));
    }

   private:
    Error error_;
  };

  template <typename T, typename Next, typename Error, typename Complete>
  class WithErrorAndComplete : public WithError<T, Next, Error> {
   public:
    WithErrorAndComplete(Next next, Error error, Complete complete)
        : WithError<T, Next, Error>(
              std::move(next),
              std::move(error)),
          complete_(std::move(complete)) {}

    void onComplete() override {
      complete_();
    }

   private:
    Complete complete_;
  };

  Observers() = delete;
};

} // observable
} // yarpl
