#pragma once

#include <exception>
#include <limits>

#include "../Observable.h"
#include "Observer.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace observable {

/// Helper methods for constructing subscriber instances from functions:
/// one, two, or three functions (callables; can be lamda, for instance)
/// may be specified, corresponding to onNext, onError and onSubscribe
/// method bodies in the subscriber.
class Observers {
 public:
  template <
      typename T,
      typename Next,
      typename = typename std::enable_if<
          std::is_callable<Next(const T&), void>::value>::type>
  static auto create(
      Next&& next) {
    return Reference<Observer<T>>(
        new Base<T, Next>(std::forward<Next>(next)));
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(const T&), void>::value &&
          std::is_callable<Error(const std::exception_ptr), void>::value>::type>
  static auto create(
      Next&& next,
      Error&& error) {
    return Reference<Observer<T>>(new WithError<T, Next, Error>(
        std::forward<Next>(next), std::forward<Error>(error)));
  }

  template <
      typename T,
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          std::is_callable<Next(const T&), void>::value &&
          std::is_callable<Error(const std::exception_ptr), void>::value &&
          std::is_callable<Complete(), void>::value>::type>
  static auto create(
      Next&& next,
      Error&& error,
      Complete&& complete) {
    return Reference<Observer<T>>(
        new WithErrorAndComplete<T, Next, Error, Complete>(
            std::forward<Next>(next),
            std::forward<Error>(error),
            std::forward<Complete>(complete)));
  }

 private:
  template <typename T, typename Next>
  class Base : public Observer<T> {
   public:
    Base(Next&& next)
        : next_(std::forward<Next>(next)) {}

    virtual void onSubscribe(Reference<Subscription> subscription) override {
      Observer<T>::onSubscribe(subscription);
    }

    virtual void onNext(const T& value) override {
      next_(value);
    }

   private:
    Next next_;
  };

  template <typename T, typename Next, typename Error>
  class WithError : public Base<T, Next> {
   public:
    WithError(Next&& next, Error&& error)
        : Base<T, Next>(std::forward<Next>(next)), error_(error) {}

    virtual void onError(std::exception_ptr error) override {
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
        Complete&& complete)
        : WithError<T, Next, Error>(
              std::forward<Next>(next),
              std::forward<Error>(error)),
          complete_(complete) {}

    virtual void onComplete() {
      complete_();
    }

   private:
    Complete complete_;
  };

  Observers() = delete;
};

} // observable
} // yarpl
