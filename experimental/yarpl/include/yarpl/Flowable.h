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
namespace flowable {

template <typename T, typename Function>
class UniqueFlowable : public reactivestreams_yarpl::Publisher<T> {
  // using reactivestream_yarpl to not conflict with the other reactivestreams
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  UniqueFlowable(Function&& function) : function_(std::move(function)) {}

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    (function_)(std::move(subscriber));
  }

  // TODO should we use "reference qualifiers" for a "lift(...) &&" version
  // that works differently than the normal "lift"?
  // for example, move when Flowable is an rvalue, otherwise copy or have a
  // shared_ptr?
  // It's unclear how we'd convert to a shared_ptr in this case though,
  // and copying seems like it could be a bad thing to end up doing
  // on all functions.

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

    return std::make_unique<UniqueFlowable<R, decltype(onSubscribe)>>(
        std::move(onSubscribe));
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
  };

  auto take(int64_t toTake) {
    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
  };

  auto subscribeOn(yarpl::Scheduler& scheduler) {
    return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
  };

 protected:
  UniqueFlowable() = default;
  UniqueFlowable(UniqueFlowable&&) = delete;
  UniqueFlowable(const UniqueFlowable&) = delete;
  UniqueFlowable& operator=(UniqueFlowable&&) = delete;
  UniqueFlowable& operator=(const UniqueFlowable&) = delete;

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
static auto unsafeCreateUniqueFlowable(F&& onSubscribeFunc) {
  return std::make_unique<UniqueFlowable<T, F>>(
      std::forward<F>(onSubscribeFunc));
}

class Flowable {
 public:
  Flowable() = default;
  Flowable(Flowable&&) = delete;
  Flowable(const Flowable&) = delete;
  Flowable& operator=(Flowable&&) = delete;
  Flowable& operator=(const Flowable&) = delete;

  static auto range(long start, long count) {
    return unsafeCreateUniqueFlowable<long>([start, count](auto subscriber) {
      auto s = new yarpl::flowable::sources::RangeSubscription(
          start, count, std::move(subscriber));
      s->start();
    });
  }

  template <typename T>
  static auto fromPublisher(
      std::unique_ptr<reactivestreams_yarpl::Publisher<T>> p) {
    return unsafeCreateUniqueFlowable<T>([p = std::move(p)](auto s) {
      p->subscribe(std::move(s));
    });
  }
};

} // flowable
} // yarpl