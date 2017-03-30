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
namespace flowableA {

/**
* Flowable type that is stack allocated and moves the OnSubscribe function
* each time lift is performed.
*
* This also templates the Function which means the public API is awkward.
*
* @tparam T
*/
template <typename T, typename Function>
class FlowableA : public reactivestreams_yarpl::Publisher<T> {
  // using reactivestream_yarpl to not conflict with the other reactivestreams
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  explicit FlowableA(Function&& function) : function_(std::move(function)) {}
  FlowableA(FlowableA&&) = default;
  FlowableA(const FlowableA&) = delete;
  FlowableA& operator=(FlowableA&&) = default;
  FlowableA& operator=(const FlowableA&) = delete;

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    (function_)(std::move(subscriber));
  }

  template <
      typename R,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<reactivestreams_yarpl::Subscriber<R>>),
          std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>>::value>::type>
  auto lift(F&& onSubscribeLift) {
    auto onSubscribe =
        [ f = std::move(function_), onSub = std::move(onSubscribeLift) ](
            std::unique_ptr<reactivestreams_yarpl::Subscriber<R>> s) mutable {
      f(onSub(std::move(s)));
    };

    return FlowableA<R, decltype(onSubscribe)>(std::move(onSubscribe));
  }

  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  auto map(F&& function) {
    return lift<typename std::result_of<F(T)>::type>(
        yarpl::operators::
            FlowableMapOperator<T, typename std::result_of<F(T)>::type, F>(
                std::forward<F>(function)));
  }

  auto take(int64_t toTake) {
    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
  }

  auto subscribeOn(yarpl::Scheduler& scheduler) {
    return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
  }

  std::unique_ptr<FlowableA<T, Function>> as_unique_ptr() {
    return std::make_unique<FlowableA<T, Function>>(std::move(function_));
  }

 private:
  Function function_;
};

/**
* Create a UniqueFlowable.
*
* Called 'unsafeCreate' since this API is not the preferred public API
* as it is easy to get wrong.
*
* This ONLY holds the function until the UniqueFlowable goes out of scope.
* It is RECOMMENDED to NOT capture state inside the function.
*
* Use Flowable::create instead unless creating internal library operators.
*
* @tparam F
* @param onSubscribeFunc
* @return
*/
template <
    typename T,
    typename F,
    typename = typename std::enable_if<std::is_callable<
        F(std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>),
        void>::value>::type>
static auto unsafeCreateUniqueFlowableA(F&& onSubscribeFunc) {
  return FlowableA<T, F>(std::forward<F>(onSubscribeFunc));
}

class FlowablesA {
 public:
  FlowablesA() = default;
  FlowablesA(FlowablesA&&) = delete;
  FlowablesA(const FlowablesA&) = delete;
  FlowablesA& operator=(FlowablesA&&) = delete;
  FlowablesA& operator=(const FlowablesA&) = delete;

  static auto range(long start, long count) {
    return unsafeCreateUniqueFlowableA<long>([start, count](auto subscriber) {
      auto s = new yarpl::flowable::sources::RangeSubscription(
          start, count, std::move(subscriber));
      s->start();
    });
  }

  template <typename T>
  static auto fromPublisher(
      std::unique_ptr<reactivestreams_yarpl::Publisher<T>> p) {
    return unsafeCreateUniqueFlowableA<T>([p = std::move(p)](auto s) {
      p->subscribe(std::move(s));
    });
  }
};

} // flowable
} // yarpl
