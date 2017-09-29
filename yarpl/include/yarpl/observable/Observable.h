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

namespace yarpl {
namespace observable {

/**
*Strategy for backpressure when converting from Observable to Flowable.
*/
enum class BackpressureStrategy { BUFFER, DROP, ERROR, LATEST, MISSING };

template <typename T>
class Observable : public virtual Refcounted, public yarpl::enable_get_ref {
 public:
  virtual Reference<Subscription> subscribe(Reference<Observer<T>>) = 0;

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename =
          typename std::enable_if<std::is_callable<Next(T), void>::value>::type>
  Reference<Subscription> subscribe(Next next) {
    return subscribe(Observers::create<T>(std::move(next)));
  }

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(folly::exception_wrapper), void>::value>::type>
  Reference<Subscription> subscribe(Next next, Error error) {
    return subscribe(Observers::create<T>(
        std::move(next), std::move(error)));
  }

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(folly::exception_wrapper), void>::value &&
          std::is_callable<Complete(), void>::value>::type>
  Reference<Subscription> subscribe(Next next, Error error, Complete complete) {
    return subscribe(Observers::create<T>(
        std::move(next),
        std::move(error),
        std::move(complete)));
  }

  Reference<Subscription> subscribe() {
    return subscribe(Observers::createNull<T>());
  }

  template <typename OnSubscribe>
  static Reference<Observable<T>> create(OnSubscribe);

  template <
      typename Function,
      typename R = typename std::result_of<Function(T)>::type>
  Reference<Observable<R>> map(Function function);

  template <typename Function>
  Reference<Observable<T>> filter(Function function);

  template <
      typename Function,
      typename R = typename std::result_of<Function(T, T)>::type>
  Reference<Observable<R>> reduce(Function function);

  Reference<Observable<T>> take(int64_t);

  Reference<Observable<T>> skip(int64_t);

  Reference<Observable<T>> ignoreElements();

  Reference<Observable<T>> subscribeOn(folly::Executor&);

  // function is invoked when onComplete occurs.
  template <typename Function>
  Reference<Observable<T>> doOnSubscribe(Function function);

  // function is invoked when onNext occurs.
  template <typename Function>
  Reference<Observable<T>> doOnNext(Function function);

  // function is invoked when onError occurs.
  template <typename Function>
  Reference<Observable<T>> doOnError(Function function);

  // function is invoked when onComplete occurs.
  template <typename Function>
  Reference<Observable<T>> doOnComplete(Function function);

  // function is invoked when either onComplete or onError occurs.
  template <typename Function>
  Reference<Observable<T>> doOnTerminate(Function function);

  // the function is invoked for each of onNext, onCompleted, onError
  template <typename Function>
  Reference<Observable<T>> doOnEach(Function function);

  // the callbacks will be invoked of each of the signals
  template <typename OnNextFunc, typename OnCompleteFunc>
  Reference<Observable<T>> doOn(OnNextFunc onNext, OnCompleteFunc onComplete);

  // the callbacks will be invoked of each of the signals
  template <typename OnNextFunc, typename OnCompleteFunc, typename OnErrorFunc>
  Reference<Observable<T>> doOn(OnNextFunc onNext, OnCompleteFunc onComplete, OnErrorFunc onError);

  /**
  * Convert from Observable to Flowable with a given BackpressureStrategy.
  *
  * Currently the only strategy is DROP.
  */
  auto toFlowable(BackpressureStrategy strategy);
};
} // observable
} // yarpl

#include "yarpl/observable/ObservableOperator.h"

namespace yarpl {
namespace observable {

template <typename T>
template <typename OnSubscribe>
Reference<Observable<T>> Observable<T>::create(OnSubscribe function) {
  static_assert(
      std::is_callable<OnSubscribe(Reference<Observer<T>>), void>(),
      "OnSubscribe must have type `void(Reference<Observer<T>>)`");

  return make_ref<FromPublisherOperator<T, OnSubscribe>>(
      std::move(function));
}

template <typename T>
template <typename Function, typename R>
Reference<Observable<R>> Observable<T>::map(Function function) {
  return make_ref<MapOperator<T, R, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::filter(Function function) {
  return make_ref<FilterOperator<T, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
template <typename Function, typename R>
Reference<Observable<R>> Observable<T>::reduce(Function function) {
  return make_ref<ReduceOperator<T, R, Function>>(
      this->ref_from_this(this), std::move(function));
}

template <typename T>
Reference<Observable<T>> Observable<T>::take(int64_t limit) {
  return make_ref<TakeOperator<T>>(this->ref_from_this(this), limit);
}

template <typename T>
Reference<Observable<T>> Observable<T>::skip(int64_t offset) {
  return make_ref<SkipOperator<T>>(this->ref_from_this(this), offset);
}

template <typename T>
Reference<Observable<T>> Observable<T>::ignoreElements() {
  return make_ref<IgnoreElementsOperator<T>>(this->ref_from_this(this));
}

template <typename T>
Reference<Observable<T>> Observable<T>::subscribeOn(folly::Executor& executor) {
  return make_ref<SubscribeOnOperator<T>>(this->ref_from_this(this), executor);
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::doOnSubscribe(Function function) {
  return details::createDoOperator(ref_from_this(this), std::move(function), [](const T&){}, [](const auto&){}, []{});
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::doOnNext(Function function) {
  return details::createDoOperator(ref_from_this(this), []{}, std::move(function), [](const auto&){}, []{});
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::doOnError(Function function) {
  return details::createDoOperator(ref_from_this(this), []{}, [](const T&){}, std::move(function), []{});
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::doOnComplete(Function function) {
  return details::createDoOperator(ref_from_this(this), []{}, [](const T&){}, [](const auto&){}, std::move(function));
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::doOnTerminate(Function function) {
  auto sharedFunction = std::make_shared<Function>(std::move(function));
  return details::createDoOperator(ref_from_this(this), []{}, [](const T&){}, [sharedFunction](const auto&){(*sharedFunction)();}, [sharedFunction](){(*sharedFunction)();});

}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::doOnEach(Function function) {
  auto sharedFunction = std::make_shared<Function>(std::move(function));
  return details::createDoOperator(ref_from_this(this), []{}, [sharedFunction](const T&){(*sharedFunction)();}, [sharedFunction](const auto&){(*sharedFunction)();}, [sharedFunction](){(*sharedFunction)();});
}

template <typename T>
template <typename OnNextFunc, typename OnCompleteFunc>
Reference<Observable<T>> Observable<T>::doOn(OnNextFunc onNext, OnCompleteFunc onComplete) {
  return details::createDoOperator(ref_from_this(this), []{}, std::move(onNext), [](const auto&){}, std::move(onComplete));
}

template <typename T>
template <typename OnNextFunc, typename OnCompleteFunc, typename OnErrorFunc>
Reference<Observable<T>> Observable<T>::doOn(OnNextFunc onNext, OnCompleteFunc onComplete, OnErrorFunc onError) {
  return details::createDoOperator(ref_from_this(this), []{}, std::move(onNext), std::move(onError), std::move(onComplete));
}

template <typename T>
auto Observable<T>::toFlowable(BackpressureStrategy strategy) {
  // we currently ONLY support the DROP strategy
  // so do not use the strategy parameter for anything
  return yarpl::flowable::Flowables::fromPublisher<T>([
    thisObservable = this->ref_from_this(this),
    strategy
  ](Reference<flowable::Subscriber<T>> subscriber) {
    Reference<flowable::Subscription> subscription;
    switch (strategy) {
      case BackpressureStrategy::DROP:
        subscription =
            make_ref<flowable::details::
                         FlowableFromObservableSubscriptionDropStrategy<T>>(
                thisObservable, subscriber);
        break;
      case BackpressureStrategy::ERROR:
        subscription =
            make_ref<flowable::details::
                         FlowableFromObservableSubscriptionErrorStrategy<T>>(
                thisObservable, subscriber);
        break;
      case BackpressureStrategy::BUFFER:
        subscription =
            make_ref<flowable::details::
                         FlowableFromObservableSubscriptionBufferStrategy<T>>(
                thisObservable, subscriber);
        break;
      case BackpressureStrategy::LATEST:
        subscription =
            make_ref<flowable::details::
                         FlowableFromObservableSubscriptionLatestStrategy<T>>(
                thisObservable, subscriber);
        break;
      case BackpressureStrategy::MISSING:
        subscription =
            make_ref<flowable::details::
                         FlowableFromObservableSubscriptionMissingStrategy<T>>(
                thisObservable, subscriber);
        break;
      default:
        CHECK(false); // unknown value for strategy
    }
    subscriber->onSubscribe(std::move(subscription));
  });
}

} // observable
} // yarpl
