// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "yarpl/Scheduler.h"
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
enum class BackpressureStrategy { DROP };

template <typename T>
class Observable : public virtual Refcounted {
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

  Reference<Observable<T>> subscribeOn(Scheduler&);

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
      get_ref(this), std::move(function));
}

template <typename T>
template <typename Function>
Reference<Observable<T>> Observable<T>::filter(Function function) {
  return make_ref<FilterOperator<T, Function>>(
      get_ref(this), std::move(function));
}

template <typename T>
template <typename Function, typename R>
Reference<Observable<R>> Observable<T>::reduce(Function function) {
  return make_ref<ReduceOperator<T, R, Function>>(
      get_ref(this), std::move(function));
}

template <typename T>
Reference<Observable<T>> Observable<T>::take(int64_t limit) {
  return make_ref<TakeOperator<T>>(get_ref(this), limit);
}

template <typename T>
Reference<Observable<T>> Observable<T>::skip(int64_t offset) {
  return make_ref<SkipOperator<T>>(get_ref(this), offset);
}

template <typename T>
Reference<Observable<T>> Observable<T>::ignoreElements() {
  return make_ref<IgnoreElementsOperator<T>>(get_ref(this));
}

template <typename T>
Reference<Observable<T>> Observable<T>::subscribeOn(Scheduler& scheduler) {
  return make_ref<SubscribeOnOperator<T>>(
      get_ref(this), scheduler);
}

template <typename T>
auto Observable<T>::toFlowable(BackpressureStrategy strategy) {
  // we currently ONLY support the DROP strategy
  // so do not use the strategy parameter for anything
  auto o = get_ref<Observable<T>>(this);
  return yarpl::flowable::Flowables::fromPublisher<T>([
    o = std::move(o), // the Observable to pass through
    strategy
  ](Reference<yarpl::flowable::Subscriber<T>> s) {
    s->onSubscribe(
        make_ref<
            yarpl::flowable::sources::FlowableFromObservableSubscription<T>>(
            std::move(o), std::move(s)));
  });
}

} // observable
} // yarpl
