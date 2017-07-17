// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/utils/type_traits.h"

#include "yarpl/single/SingleObserver.h"

namespace yarpl {
namespace single {

/// Helper methods for constructing subscriber instances from functions:
/// one or two functions (callables; can be lamda, for instance)
/// may be specified, corresponding to onNext, onError and onComplete
/// method bodies in the subscriber.
class SingleObservers {
 private:
  /// Defined if Success and Error are signature-compatible with
  /// onSuccess and onError subscriber methods respectively.
  template <
      typename T,
      typename Success,
      typename Error = void (*)(std::exception_ptr)>
  using EnableIfCompatible = typename std::enable_if<
      std::is_callable<Success(T), void>::value &&
      std::is_callable<Error(std::exception_ptr), void>::value>::type;

 public:
  template <typename T, typename Next, typename = EnableIfCompatible<T, Next>>
  static auto create(Next&& next) {
    return Reference<SingleObserver<T>>(
        new Base<T, Next>(std::forward<Next>(next)));
  }

  template <
      typename T,
      typename Success,
      typename Error,
      typename = EnableIfCompatible<T, Success, Error>>
  static auto create(Success&& next, Error&& error) {
    return Reference<SingleObserver<T>>(new WithError<T, Success, Error>(
        std::forward<Success>(next), std::forward<Error>(error)));
  }

 private:
  template <typename T, typename Next>
  class Base : public SingleObserver<T> {
   public:
    explicit Base(Next&& next) : next_(std::forward<Next>(next)) {}

    void onSuccess(T value) override {
      next_(std::move(value));
      // TODO how do we call the super to trigger release?
      //      SingleObserver<T>::onSuccess(value);
    }

   private:
    Next next_;
  };

  template <typename T, typename Success, typename Error>
  class WithError : public Base<T, Success> {
   public:
    WithError(Success&& next, Error&& error)
        : Base<T, Success>(std::forward<Success>(next)), error_(error) {}

    void onError(std::exception_ptr error) override {
      error_(error);
      // TODO do we call the super here to trigger release?
      Base<T, Success>::onError(error);
    }

   private:
    Error error_;
  };

  SingleObservers() = delete;
};

} // observable
} // yarpl
