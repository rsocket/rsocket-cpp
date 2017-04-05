// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

#include "yarpl/flowable/operators/Flowable_Map.h"
#include "yarpl/flowable/operators/Flowable_SubscribeOn.h"
#include "yarpl/flowable/operators/Flowable_Take.h"

namespace yarpl {
namespace flowableC {

/**
 * Flowable type that uses unique_ptr for memory management.
 *
 * Does not work correctly in all chaining cases as OnSubscribe
 * function can be destroyed. See Flowable_lifecycle.cpp test.
 *
 * @tparam T
 */
template <typename T>
class FlowableC : public reactivestreams_yarpl::Publisher<T> {
  // using reactivestream_yarpl to not conflict with the other reactivestreams
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  FlowableC(FlowableC&&) = delete;
  FlowableC(const FlowableC&) = delete;
  FlowableC& operator=(FlowableC&&) = delete;
  FlowableC& operator=(const FlowableC&) = delete;

  /**
   * Create a F<T> with an onSubscribe function that takes S<T>
   *
   * @tparam F
   * @param function
   * @return
   */
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(std::unique_ptr<Subscriber>), void>::value>::type>
  static std::unique_ptr<FlowableC<T>> create(F&& function) {
    return std::make_unique<Derived<F>>(std::forward<F>(function));
  }

  /**
   * Lift an operator into F<T> and return F<R>
   * @tparam R
   * @tparam F
   * @param onSubscribeLift
   * @return
   */
  template <
      typename R,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<reactivestreams_yarpl::Subscriber<R>>),
          std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>>::value>::type>
  std::unique_ptr<FlowableC<R>> lift(F&& onSubscribeLift) {
    return FlowableC<R>::create(
        [ this, onSub = std::move(onSubscribeLift) ](auto sOfR) mutable {
          this->subscribe(std::move(onSub(std::move(sOfR))));
        });
  }

  /**
   * Map F<T> -> F<R>
   *
   * @tparam F
   * @param function
   * @return
   */
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  std::unique_ptr<FlowableC<typename std::result_of<F(T)>::type>> map(
      F&& function);

  /**
   * Take n items from F<T> then cancel.
   * @param toTake
   * @return
   */
  std::unique_ptr<FlowableC<T>> take(int64_t toTake) {
    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
  }

  /**
   * SubscribeOn the given Scheduler
   * @param scheduler
   * @return
   */
  std::unique_ptr<FlowableC<T>> subscribeOn(yarpl::Scheduler& scheduler) {
    return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
  }

 protected:
  FlowableC() = default;

 private:
  template <typename Function>
  class Derived;
};

template <typename T>
template <typename Function>
class FlowableC<T>::Derived : public FlowableC<T> {
 public:
  explicit Derived(Function&& function)
      : function_(
            std::make_unique<Function>(std::forward<Function>(function))) {}

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    (*function_)(std::move(subscriber));
  }

 private:
  std::unique_ptr<Function> function_;
};

template <typename T>
template <typename F, typename Default>
std::unique_ptr<FlowableC<typename std::result_of<F(T)>::type>>
FlowableC<T>::map(F&& function) {
  return lift<typename std::result_of<F(T)>::type>(
      yarpl::operators::
          FlowableMapOperator<T, typename std::result_of<F(T)>::type, F>(
              std::forward<F>(function)));
}

class FlowablesC {
 public:
  FlowablesC() = default;
  FlowablesC(FlowablesC&&) = delete;
  FlowablesC(const FlowablesC&) = delete;
  FlowablesC& operator=(FlowablesC&&) = delete;
  FlowablesC& operator=(const FlowablesC&) = delete;

  static std::unique_ptr<FlowableC<long>> range(long start, long count) {
    return FlowableC<long>::create([start, count](auto subscriber) {
      auto s = new yarpl::flowable::sources::RangeSubscription(
          start, count, std::move(subscriber));
      s->start();
    });
  }
};
}
}
