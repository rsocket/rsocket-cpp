// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/utils/type_traits.h"

#include "yarpl/single/SingleObserver.h"

#include <folly/functional/Invoke.h>

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
      typename Error = void (*)(folly::exception_wrapper)>
  using EnableIfCompatible = typename std::enable_if<
      folly::is_invocable<Success, T>::value &&
      folly::is_invocable<Error, folly::exception_wrapper>::value>::type;

 public:
  template <typename T, typename Next, typename = EnableIfCompatible<T, Next>>
  static auto create(Next next) {
    return make_ref<Base<T, Next>, SingleObserverBase<T>>(std::move(next));
  }

  template <
      typename T,
      typename Success,
      typename Error,
      typename = EnableIfCompatible<T, Success, Error>>
  static auto create(Success next, Error error) {
    return make_ref<WithError<T, Success, Error>, SingleObserverBase<T>>(
        std::move(next), std::move(error));
  }

 private:
  template <typename T, typename Next>
  class Base : public SingleObserverBase<T> {
   public:
    explicit Base(Next next) : next_(std::move(next)) {}

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
    WithError(Success next, Error error)
        : Base<T, Success>(std::move(next)), error_(std::move(error)) {}

    void onError(folly::exception_wrapper error) override {
      error_(error);
      // TODO do we call the super here to trigger release?
      Base<T, Success>::onError(std::move(error));
    }

   private:
    Error error_;
  };

  SingleObservers() = delete;
};

} // observable
} // yarpl
