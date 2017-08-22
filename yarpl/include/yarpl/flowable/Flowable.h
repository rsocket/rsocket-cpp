// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>

#include <glog/logging.h>

#include "yarpl/Refcounted.h"
#include "yarpl/Scheduler.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscribers.h"
#include "yarpl/utils/credits.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace flowable {

template <typename T>
class Flowable : public virtual Refcounted {
 public:
  virtual void subscribe(Reference<Subscriber<T>>) = 0;

  /**
   * Subscribe overload that accepts lambdas.
   */
  template <
      typename Next,
      typename =
          typename std::enable_if<std::is_callable<Next(T), void>::value>::type>
  void subscribe(Next next, int64_t batch = credits::kNoFlowControl) {
    subscribe(Subscribers::create<T>(std::move(next), batch));
  }

  /**
   * Subscribe overload that accepts lambdas.
   *
   * Takes an optional batch size for request_n. Default is no flow control.
   */
  template <
      typename Next,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(folly::exception_wrapper), void>::value>::type>
  void subscribe(
      Next next,
      Error error,
      int64_t batch = credits::kNoFlowControl) {
    subscribe(Subscribers::create<T>(
        std::move(next), std::move(error), batch));
  }

  /**
   * Subscribe overload that accepts lambdas.
   *
   * Takes an optional batch size for request_n. Default is no flow control.
   */
  template <
      typename Next,
      typename Error,
      typename Complete,
      typename = typename std::enable_if<
          std::is_callable<Next(T), void>::value &&
          std::is_callable<Error(folly::exception_wrapper), void>::value &&
          std::is_callable<Complete(), void>::value>::type>
  void subscribe(
      Next next,
      Error error,
      Complete complete,
      int64_t batch = credits::kNoFlowControl) {
    subscribe(Subscribers::create<T>(
        std::move(next),
        std::move(error),
        std::move(complete),
        batch));
  }

  template <typename Function, typename R = typename std::result_of<Function(T)>::type>
  Reference<Flowable<R>> map(Function function);

  template <typename Function>
  Reference<Flowable<T>> filter(Function function);

  template <typename Function, typename R = typename std::result_of<Function(T, T)>::type>
  Reference<Flowable<R>> reduce(Function function);

  Reference<Flowable<T>> take(int64_t);

  Reference<Flowable<T>> skip(int64_t);

  Reference<Flowable<T>> ignoreElements();

  Reference<Flowable<T>> subscribeOn(Scheduler&);

  template <
      typename Emitter,
      typename = typename std::enable_if<std::is_callable<
          Emitter(Reference<Subscriber<T>>, int64_t),
          std::tuple<int64_t, bool>>::value>::type>
  static Reference<Flowable<T>> create(Emitter emitter);
};

} // flowable
} // yarpl

#include "yarpl/flowable/EmitterFlowable.h"
#include "yarpl/flowable/FlowableOperator.h"

namespace yarpl {
namespace flowable {

template <typename T>
template <typename Emitter, typename>
Reference<Flowable<T>> Flowable<T>::create(Emitter emitter) {
  return make_ref<details::EmitterWrapper<T, Emitter>>(std::move(emitter));
}

template <typename T>
template <typename Function, typename R>
Reference<Flowable<R>> Flowable<T>::map(Function function) {
  return make_ref<MapOperator<T, R, Function>>(
      get_ref(this), std::move(function));
}

template <typename T>
template <typename Function>
Reference<Flowable<T>> Flowable<T>::filter(Function function) {
  return make_ref<FilterOperator<T, Function>>(
      get_ref(this), std::move(function));
}

template <typename T>
template <typename Function, typename R>
Reference<Flowable<R>> Flowable<T>::reduce(Function function) {
  return make_ref<ReduceOperator<T, R, Function>>(
      get_ref(this), std::move(function));
}

template <typename T>
Reference<Flowable<T>> Flowable<T>::take(int64_t limit) {
  return make_ref<TakeOperator<T>>(get_ref(this), limit);
}

template <typename T>
Reference<Flowable<T>> Flowable<T>::skip(int64_t offset) {
  return make_ref<SkipOperator<T>>(get_ref(this), offset);
}

template <typename T>
Reference<Flowable<T>> Flowable<T>::ignoreElements() {
  return make_ref<IgnoreElementsOperator<T>>(get_ref(this));
}

template <typename T>
Reference<Flowable<T>> Flowable<T>::subscribeOn(Scheduler& scheduler) {
  return make_ref<SubscribeOnOperator<T>>(get_ref(this), scheduler);
}

} // flowable
} // yarpl
