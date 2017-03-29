// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
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
namespace flowableB {

/**
* Flowable type that is stack allocated and moves the OnSubscribe function
* each time lift is performed.
*
* This uses std::function instead of templated Function to make the public API
* better than FlowableB. A side-effect is that Functors/OnSubscribe must
* support copy.
*
* @tparam T
*/
template <typename T>
class FlowableB : public reactivestreams_yarpl::Publisher<T> {
  // using reactivestream_yarpl to not conflict with the other reactivestreams
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  explicit FlowableB(std::function<void(std::unique_ptr<Subscriber>)>&& function)
      : function_(std::move(function)) {}
  FlowableB(FlowableB&&) = default;
  FlowableB(const FlowableB&) = delete;
  FlowableB& operator=(FlowableB&&) = default;
  FlowableB& operator=(const FlowableB&) = delete;

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

    return FlowableB<R>(std::move(onSubscribe));
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

 private:
  std::function<void(std::unique_ptr<Subscriber>)> function_;
};

class FlowablesB {
 public:
  FlowablesB() = default;
  FlowablesB(FlowablesB&&) = delete;
  FlowablesB(const FlowablesB&) = delete;
  FlowablesB& operator=(FlowablesB&&) = delete;
  FlowablesB& operator=(const FlowablesB&) = delete;

  static auto range(long start, long count) {
    return FlowableB<long>([start, count](auto subscriber) {
      auto s = new yarpl::flowable::sources::RangeSubscription(
          start, count, std::move(subscriber));
      s->start();
    });
  }

  template <typename T>
  static auto fromPublisher(
      std::unique_ptr<reactivestreams_yarpl::Publisher<T>> p) {
    return FlowableB<T>([p = std::move(p)](auto s) {
      p->subscribe(std::move(s));
    });
  }
};

} // flowable
} // yarpl
