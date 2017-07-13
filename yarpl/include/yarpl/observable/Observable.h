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
  virtual void subscribe(Reference<Observer<T>>) = 0;

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename =
          typename std::enable_if<std::is_callable<Next(T), void>::value>::type>
  void subscribe(Next&& next) {
    subscribe(Observers::create<T>(next));
  }

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(std::exception_ptr), void>::value>::type>
  void subscribe(Next&& next, Error&& error) {
    subscribe(Observers::create<T>(next, error));
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
          std::is_callable<Error(std::exception_ptr), void>::value &&
          std::is_callable<Complete(), void>::value>::type>
  void subscribe(Next&& next, Error&& error, Complete&& complete) {
    subscribe(Observers::create<T>(next, error, complete));
  }

  template <typename OnSubscribe>
  static auto create(OnSubscribe&&);

  template <typename Function>
  auto map(Function&& function);

  template <typename Function>
  auto filter(Function&& function);

  template <typename Function>
  auto reduce(Function&& function);

  auto take(int64_t);

  auto skip(int64_t);

  auto ignoreElements();

  auto subscribeOn(Scheduler&);

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
auto Observable<T>::create(OnSubscribe&& function) {
  static_assert(
      std::is_callable<OnSubscribe(Reference<Observer<T>>), void>(),
      "OnSubscribe must have type `void(Reference<Observer<T>>)`");

  return make_ref<FromPublisherOperator<T, OnSubscribe>>(
      std::forward<OnSubscribe>(function));
}

template <typename T>
template <typename Function>
auto Observable<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return Reference<Observable<D>>(new MapOperator<T, D, Function>(
      Reference<Observable<T>>(this), std::forward<Function>(function)));
}

template <typename T>
template <typename Function>
auto Observable<T>::filter(Function&& function) {
  return Reference<Observable<T>>(new FilterOperator<T, Function>(
      Reference<Observable<T>>(this), std::forward<Function>(function)));
}

template <typename T>
template <typename Function>
auto Observable<T>::reduce(Function&& function) {
  using D = typename std::result_of<Function(T, T)>::type;
  return Reference<Observable<D>>(new ReduceOperator<T, D, Function>(
      Reference<Observable<T>>(this), std::forward<Function>(function)));
}

template <typename T>
auto Observable<T>::take(int64_t limit) {
  return Reference<Observable<T>>(
      new TakeOperator<T>(Reference<Observable<T>>(this), limit));
}

template <typename T>
auto Observable<T>::skip(int64_t offset) {
  return Reference<Observable<T>>(
      new SkipOperator<T>(Reference<Observable<T>>(this), offset));
}

template <typename T>
auto Observable<T>::ignoreElements() {
  return Reference<Observable<T>>(
      new IgnoreElementsOperator<T>(Reference<Observable<T>>(this)));
}

template <typename T>
auto Observable<T>::subscribeOn(Scheduler& scheduler) {
  return Reference<Observable<T>>(
      new SubscribeOnOperator<T>(Reference<Observable<T>>(this), scheduler));
}

template <typename T>
auto Observable<T>::toFlowable(BackpressureStrategy strategy) {
  // we currently ONLY support the DROP strategy
  // so do not use the strategy parameter for anything
  auto o = Reference<Observable<T>>(this);
  return yarpl::flowable::Flowables::fromPublisher<T>([
    o = std::move(o), // the Observable to pass through
    strategy
  ](Reference<yarpl::flowable::Subscriber<T>> s) {
    s->onSubscribe(Reference<yarpl::flowable::Subscription>(
        new yarpl::flowable::sources::FlowableFromObservableSubscription<T>(
            std::move(o), std::move(s))));
  });
}

} // observable
} // yarpl
