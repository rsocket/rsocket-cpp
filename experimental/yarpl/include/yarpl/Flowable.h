// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

#include "yarpl/flowable/operators/Flowable_Map.h"
#include "yarpl/flowable/operators/Flowable_SubscribeOn.h"
#include "yarpl/flowable/operators/Flowable_Take.h"

namespace yarpl {
namespace flowable {

/**
 * Flowable type that uses shared_ptr for memory management.
 *
 * @tparam T
 */
template <typename T>
class Flowable : public reactivestreams_yarpl::Publisher<T>,
                 public std::enable_shared_from_this<Flowable<T>> {
  // using reactivestream_yarpl to not conflict with the other reactivestreams
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  Flowable(Flowable&&) = delete;
  Flowable(const Flowable&) = delete;
  Flowable& operator=(Flowable&&) = delete;
  Flowable& operator=(const Flowable&) = delete;

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
  static std::shared_ptr<Flowable<T>> create(F&& function) {
    // TODO(vjn): figure out why clang complains about the cast of the shared
    // pointer to the base class.  (Also fails with std::static_pointer_cast.)
    // Meantime, not using std::make_shared.
    Flowable* base = new Derived<F>(std::forward<F>(function));
    return std::shared_ptr<Flowable>(base);
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
  std::shared_ptr<Flowable<R>> lift(F&& onSubscribeLift) {
    return Flowable<R>::create([
      shared_this = this->shared_from_this(),
      onSub = std::move(onSubscribeLift)
    ](auto sOfR) mutable {
      shared_this->subscribe(std::move(onSub(std::move(sOfR))));
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
  std::shared_ptr<Flowable<typename std::result_of<F(T)>::type>> map(
      F&& function) {
    return lift<typename std::result_of<F(T)>::type>(
        yarpl::operators::
            FlowableMapOperator<T, typename std::result_of<F(T)>::type, F>(
                std::forward<F>(function)));
  }

  /**
   * Take n items from F<T> then cancel.
   * @param toTake
   * @return
   */
  std::shared_ptr<Flowable<T>> take(int64_t toTake) {
    return lift<T>(yarpl::operators::FlowableTakeOperator<T>(toTake));
  }

  /**
   * SubscribeOn the given Scheduler
   * @param scheduler
   * @return
   */
  std::shared_ptr<Flowable<T>> subscribeOn(yarpl::Scheduler& scheduler) {
    return lift<T>(yarpl::operators::FlowableSubscribeOnOperator<T>(scheduler));
  }

 protected:
  Flowable() = default;

 private:
   template<typename Function>
   class Derived;
};

template<typename T>
template <typename Function>
class Flowable<T>::Derived : public Flowable<T> {
 public:
  explicit Derived(Function&& function)
      : function_(std::forward<Function>(function)) {}

  void subscribe(std::unique_ptr<Subscriber> subscriber) override {
    (function_)(std::move(subscriber));
  }

 private:
  Function function_;
};

class Flowables {
 public:
  Flowables() = default;
  Flowables(Flowables&&) = delete;
  Flowables(const Flowables&) = delete;
  Flowables& operator=(Flowables&&) = delete;
  Flowables& operator=(const Flowables&) = delete;

  /**
   * Create a F<T> with an onSubscribe function that takes S<T>
   *
   * @tparam F
   * @param function
   * @return
   */
  template <
      typename T,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<reactivestreams_yarpl::Subscriber<T>>),
          void>::value>::type>
  static std::unique_ptr<Flowable<T>> create(F&& function) {
    return Flowable<T>::create(std::forward<F>(function));
  }

  static std::shared_ptr<Flowable<long>> range(long start, long count) {
    return Flowable<long>::create([start, count](auto subscriber) {
      auto s = new yarpl::flowable::sources::RangeSubscription(
          start, count, std::move(subscriber));
      s->start();
    });
  }

  template <typename T>
  static auto fromPublisher(
      std::shared_ptr<reactivestreams_yarpl::Publisher<T>> p) {
    return Flowable<T>::create([p = std::move(p)](auto s) {
      p->subscribe(std::move(s));
    });
  }
};
}
}
