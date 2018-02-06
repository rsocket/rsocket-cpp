// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "yarpl/utils/type_traits.h"

#include "yarpl/Refcounted.h"
#include "yarpl/observable/Observer.h"
#include "yarpl/observable/Observers.h"
#include "yarpl/observable/Subscription.h"

#include "yarpl/Flowable.h"
#include "yarpl/flowable/Flowable_FromObservable.h"

#include <folly/functional/Invoke.h>

namespace yarpl {
namespace observable {

/**
 *Strategy for backpressure when converting from Observable to Flowable.
 */
enum class BackpressureStrategy { BUFFER, DROP, ERROR, LATEST, MISSING };

template <typename T = void>
class Observable : public yarpl::enable_get_ref {
 public:
  static std::shared_ptr<Observable<T>> empty() {
    auto lambda = [](std::shared_ptr<Observer<T>> observer) {
      observer->onComplete();
    };
    return Observable<T>::create(std::move(lambda));
  }

  static std::shared_ptr<Observable<T>> error(folly::exception_wrapper ex) {
    auto lambda = [ex = std::move(ex)](std::shared_ptr<Observer<T>> observer) {
      observer->onError(std::move(ex));
    };
    return Observable<T>::create(std::move(lambda));
  }

  template <typename Ex>
  static std::shared_ptr<Observable<T>> error(Ex&) {
    static_assert(
        std::is_lvalue_reference<Ex>::value,
        "use variant of error() method accepting also exception_ptr");
  }

  template <typename Ex>
  static std::shared_ptr<Observable<T>> error(Ex& ex, std::exception_ptr ptr) {
    auto lambda = [ew = folly::exception_wrapper(std::move(ptr), ex)](
                      std::shared_ptr<Observer<T>> observer) {
      observer->onError(std::move(ew));
    };
    return Observable<T>::create(std::move(lambda));
  }

  /**
   * The Defer operator waits until an observer subscribes to it, and then it
   * generates an Observable with an ObservableFactory function. It
   * does this afresh for each subscriber, so although each subscriber may
   * think it is subscribing to the same Observable, in fact each subscriber
   * gets its own individual sequence.
   */
  template <
      typename ObservableFactory,
      typename = typename std::enable_if<folly::is_invocable_r<
          std::shared_ptr<Observable<T>>,
          ObservableFactory>::value>::type>
  static std::shared_ptr<Observable<T>> defer(ObservableFactory);

  template <typename OnSubscribe>
  static std::shared_ptr<Observable<T>> create(OnSubscribe);

  virtual std::shared_ptr<Subscription> subscribe(
      std::shared_ptr<Observer<T>>) = 0;

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename =
          typename std::enable_if<folly::is_invocable<Next, T>::value>::type>
  std::shared_ptr<Subscription> subscribe(Next next) {
    return subscribe(Observers::create<T>(std::move(next)));
  }

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          folly::is_invocable<Next, T>::value &&
          folly::is_invocable<Error, folly::exception_wrapper>::value>::type>
  std::shared_ptr<Subscription> subscribe(Next next, Error error) {
    return subscribe(Observers::create<T>(std::move(next), std::move(error)));
  }

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          folly::is_invocable<Next, T>::value &&
          folly::is_invocable<Error, folly::exception_wrapper>::value &&
          folly::is_invocable<Complete>::value>::type>
  std::shared_ptr<Subscription>
  subscribe(Next next, Error error, Complete complete) {
    return subscribe(Observers::create<T>(
        std::move(next), std::move(error), std::move(complete)));
  }

  std::shared_ptr<Subscription> subscribe() {
    return subscribe(Observers::createNull<T>());
  }

  template <
      typename Function,
      typename R = typename std::result_of<Function(T)>::type>
  std::shared_ptr<Observable<R>> map(Function function);

  template <typename Function>
  std::shared_ptr<Observable<T>> filter(Function function);

  template <
      typename Function,
      typename R = typename std::result_of<Function(T, T)>::type>
  std::shared_ptr<Observable<R>> reduce(Function function);

  std::shared_ptr<Observable<T>> take(int64_t);

  std::shared_ptr<Observable<T>> skip(int64_t);

  std::shared_ptr<Observable<T>> ignoreElements();

  std::shared_ptr<Observable<T>> subscribeOn(folly::Executor&);

  // function is invoked when onComplete occurs.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Observable<T>> doOnSubscribe(Function function);

  // function is invoked when onNext occurs.
  template <
      typename Function,
      typename = typename std::enable_if<
          folly::is_invocable<Function, const T&>::value>::type>
  std::shared_ptr<Observable<T>> doOnNext(Function function);

  // function is invoked when onError occurs.
  template <
      typename Function,
      typename = typename std::enable_if<
          folly::is_invocable<Function, folly::exception_wrapper&>::value>::
          type>
  std::shared_ptr<Observable<T>> doOnError(Function function);

  // function is invoked when onComplete occurs.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Observable<T>> doOnComplete(Function function);

  // function is invoked when either onComplete or onError occurs.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Observable<T>> doOnTerminate(Function function);

  // the function is invoked for each of onNext, onCompleted, onError
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Observable<T>> doOnEach(Function function);

  // the callbacks will be invoked of each of the signals
  template <
      typename OnNextFunc,
      typename OnCompleteFunc,
      typename = typename std::enable_if<
          folly::is_invocable<OnNextFunc, const T&>::value>::type,
      typename = typename std::enable_if<
          folly::is_invocable<OnCompleteFunc>::value>::type>
  std::shared_ptr<Observable<T>> doOn(
      OnNextFunc onNext,
      OnCompleteFunc onComplete);

  // the callbacks will be invoked of each of the signals
  template <
      typename OnNextFunc,
      typename OnCompleteFunc,
      typename OnErrorFunc,
      typename = typename std::enable_if<
          folly::is_invocable<OnNextFunc, const T&>::value>::type,
      typename = typename std::enable_if<
          folly::is_invocable<OnCompleteFunc>::value>::type,
      typename = typename std::enable_if<
          folly::is_invocable<OnErrorFunc, folly::exception_wrapper&>::value>::
          type>
  std::shared_ptr<Observable<T>>
  doOn(OnNextFunc onNext, OnCompleteFunc onComplete, OnErrorFunc onError);

  // function is invoked when cancel is called.
  template <
      typename Function,
      typename =
          typename std::enable_if<folly::is_invocable<Function>::value>::type>
  std::shared_ptr<Observable<T>> doOnCancel(Function function);

  /**
   * Convert from Observable to Flowable with a given BackpressureStrategy.
   *
   * Currently the only strategy is DROP.
   */
  auto toFlowable(BackpressureStrategy strategy);
};
} // namespace observable
} // namespace yarpl

#include "yarpl/observable/DeferObservable.h"
#include "yarpl/observable/ObservableOperator.h"

namespace yarpl {
namespace observable {

template <typename T>
template <typename OnSubscribe>
std::shared_ptr<Observable<T>> Observable<T>::create(OnSubscribe function) {
  static_assert(
      folly::is_invocable<OnSubscribe, std::shared_ptr<Observer<T>>>::value,
      "OnSubscribe must have type `void(std::shared_ptr<Observer<T>>)`");

  return std::make_shared<FromPublisherOperator<T, OnSubscribe>>(
      std::move(function));
}

template <typename T>
template <typename ObservableFactory, typename>
std::shared_ptr<Observable<T>> Observable<T>::defer(ObservableFactory factory) {
  return std::make_shared<details::DeferObservable<T, ObservableFactory>>(
      std::move(factory));
}

template <typename T>
template <typename Function, typename R>
std::shared_ptr<Observable<R>> Observable<T>::map(Function function) {
  return std::make_shared<MapOperator<T, R, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function>
std::shared_ptr<Observable<T>> Observable<T>::filter(Function function) {
  return std::make_shared<FilterOperator<T, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function, typename R>
std::shared_ptr<Observable<R>> Observable<T>::reduce(Function function) {
  return std::make_shared<ReduceOperator<T, R, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
std::shared_ptr<Observable<T>> Observable<T>::take(int64_t limit) {
  return std::make_shared<TakeOperator<T>>(this->ref_from_this(this), limit);
}

template <typename T>
std::shared_ptr<Observable<T>> Observable<T>::skip(int64_t offset) {
  return std::make_shared<SkipOperator<T>>(this->ref_from_this(this), offset);
}

template <typename T>
std::shared_ptr<Observable<T>> Observable<T>::ignoreElements() {
  return std::make_shared<IgnoreElementsOperator<T>>(this->ref_from_this(this));
}

template <typename T>
std::shared_ptr<Observable<T>> Observable<T>::subscribeOn(
    folly::Executor& executor) {
  return std::make_shared<SubscribeOnOperator<T>>(
      this->ref_from_this(this), executor);
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnSubscribe(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      std::move(function),
      [](const T&) {},
      [](const auto&) {},
      [] {},
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnNext(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      std::move(function),
      [](const auto&) {},
      [] {},
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnError(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [](const T&) {},
      std::move(function),
      [] {},
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnComplete(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [](const T&) {},
      [](const auto&) {},
      std::move(function),
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnTerminate(Function function) {
  auto sharedFunction = std::make_shared<Function>(std::move(function));
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [](const T&) {},
      [sharedFunction](const auto&) { (*sharedFunction)(); },
      [sharedFunction]() { (*sharedFunction)(); },
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnEach(Function function) {
  auto sharedFunction = std::make_shared<Function>(std::move(function));
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      [sharedFunction](const T&) { (*sharedFunction)(); },
      [sharedFunction](const auto&) { (*sharedFunction)(); },
      [sharedFunction]() { (*sharedFunction)(); },
      [] {}); // onCancel
}

template <typename T>
template <typename OnNextFunc, typename OnCompleteFunc, typename, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOn(
    OnNextFunc onNext,
    OnCompleteFunc onComplete) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      std::move(onNext),
      [](const auto&) {},
      std::move(onComplete),
      [] {}); // onCancel
}

template <typename T>
template <
    typename OnNextFunc,
    typename OnCompleteFunc,
    typename OnErrorFunc,
    typename,
    typename,
    typename>
std::shared_ptr<Observable<T>> Observable<T>::doOn(
    OnNextFunc onNext,
    OnCompleteFunc onComplete,
    OnErrorFunc onError) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {},
      std::move(onNext),
      std::move(onError),
      std::move(onComplete),
      [] {}); // onCancel
}

template <typename T>
template <typename Function, typename>
std::shared_ptr<Observable<T>> Observable<T>::doOnCancel(Function function) {
  return details::createDoOperator(
      ref_from_this(this),
      [] {}, // onSubscribe
      [](const auto&) {}, // onNext
      [](const auto&) {}, // onError
      [] {}, // onComplete
      std::move(function)); // onCancel
}

template <typename T>
auto Observable<T>::toFlowable(BackpressureStrategy strategy) {
  // we currently ONLY support the DROP strategy
  // so do not use the strategy parameter for anything
  return yarpl::flowable::Flowable<
      T>::fromPublisher([thisObservable = this->ref_from_this(this),
                         strategy](std::shared_ptr<flowable::Subscriber<T>>
                                       subscriber) {
    std::shared_ptr<flowable::Subscription> subscription;
    switch (strategy) {
      case BackpressureStrategy::DROP:
        subscription = std::make_shared<
            flowable::details::FlowableFromObservableSubscriptionDropStrategy<
                T>>(thisObservable, subscriber);
        break;
      case BackpressureStrategy::ERROR:
        subscription = std::make_shared<
            flowable::details::FlowableFromObservableSubscriptionErrorStrategy<
                T>>(thisObservable, subscriber);
        break;
      case BackpressureStrategy::BUFFER:
        subscription = std::make_shared<
            flowable::details::FlowableFromObservableSubscriptionBufferStrategy<
                T>>(thisObservable, subscriber);
        break;
      case BackpressureStrategy::LATEST:
        subscription = std::make_shared<
            flowable::details::FlowableFromObservableSubscriptionLatestStrategy<
                T>>(thisObservable, subscriber);
        break;
      case BackpressureStrategy::MISSING:
        subscription = std::make_shared<
            flowable::details::
                FlowableFromObservableSubscriptionMissingStrategy<T>>(
            thisObservable, subscriber);
        break;
      default:
        CHECK(false); // unknown value for strategy
    }
    subscriber->onSubscribe(std::move(subscription));
  });
}

} // namespace observable
} // namespace yarpl
