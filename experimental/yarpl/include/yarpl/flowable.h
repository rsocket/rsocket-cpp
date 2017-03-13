// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "reactivestreams/ReactiveStreams.h"
#include "type_traits.h"

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
          std::is_callable<F(Subscriber&), void>::value>::type>
  static std::shared_ptr<Flowable> generate(F&& function) {
    return std::make_shared<FlowableGenerator<F>>(std::forward<F>(function));
  }
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(std::unique_ptr<Subscriber>), void>::value>::type>
  static std::shared_ptr<Flowable> create(F&& onSubscribeFunc) {
    return std::make_shared<FlowableOnSubscribeFunc<F>>(
        std::forward<F>(onSubscribeFunc));
  }

  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  auto map(F&& function);

 protected:
  Flowable() = default;

 private:
  Flowable(Flowable&&) = delete;
  Flowable(const Flowable&) = delete;
  Flowable& operator=(Flowable&&) = delete;
  Flowable& operator=(const Flowable&) = delete;

  template <typename Function>
  class FlowableGenerator : public Flowable {
   public:
    FlowableGenerator(Function&& function)
        : function_(
              std::make_shared<Function>(std::forward<Function>(function))) {}

    void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      // TODO implement this
      subscriber->onError(std::make_exception_ptr("not implemented"));
    }

   private:
    const std::shared_ptr<Function> function_;
  };

  template <typename Function>
  class FlowableOnSubscribeFunc : public Flowable {
   public:
    FlowableOnSubscribeFunc(Function&& function)
        : function_(
              std::make_shared<Function>(std::forward<Function>(function))) {}

    void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      (*function_)(std::move(subscriber));
    }

   private:
    const std::shared_ptr<Function> function_;
  };
};

} // flowable
} // yarpl

#include "map.h"

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
