// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace flowable {

// using reactivestream_yarpl to not conflict with the other reactivestreams

template <typename T>
class Flowable : public reactivestreams_yarpl::Publisher<T>,
                 public std::enable_shared_from_this<Flowable<T>> {
  using Subscriber = reactivestreams_yarpl::Subscriber<T>;
  using Subscription = reactivestreams_yarpl::Subscription;

 public:
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(std::unique_ptr<Subscriber>), void>::value>::type>
  static std::unique_ptr<Flowable> create(F&& onSubscribeFunc) {
    return std::make_unique<FlowableOnSubscribeFunc<F>>(
        std::forward<F>(onSubscribeFunc));
  }

  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  auto map(F&& function);

  static std::unique_ptr<Flowable<long>> range(long start, long count) {
    return create([start, count](auto subscriber) {
      auto s_ = new yarpl::flowable::sources::RangeSubscription(
          start, count, std::move(subscriber));
      s_->start();
    });
  }

 protected:
  Flowable() = default;
  Flowable(Flowable&&) = delete;
  Flowable(const Flowable&) = delete;
  Flowable& operator=(Flowable&&) = delete;
  Flowable& operator=(const Flowable&) = delete;

 private:
  template <typename Function>
  class FlowableOnSubscribeFunc : public Flowable {
   public:
    FlowableOnSubscribeFunc(Function&& function)
        : function_(Function(std::forward<Function>(function))) {}

    void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      (function_)(std::move(subscriber));
    }

   private:
    Function function_;
  };
};

} // flowable
} // yarpl

#include "yarpl/flowable/operators/Flowable_Map.h"

namespace yarpl {
namespace flowable {

template <typename T>
template <typename F, typename Default>
auto Flowable<T>::map(F&& function) {
  return std::make_shared<operators::Mapper<T, F>>(
      std::forward<F>(function), this->shared_from_this());
}

} // flowable
} // yarpl
