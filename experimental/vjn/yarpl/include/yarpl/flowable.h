#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <reactivestreams/publisher.h>
#include <reactivestreams/subscription.h>
#include <reactivestreams/type_traits.h>

#include "yarpl/scheduler.h"

namespace yarpl {

template <typename T>
class Flowable : public reactivestreams::Publisher<T>,
                 public std::enable_shared_from_this<Flowable<T>> {
  using Subscriber = reactivestreams::Subscriber<T>;
  using Subscription = reactivestreams::Subscription<T>;

 public:
  template <typename F,
            typename = typename std::enable_if<
                std::is_callable<F(Subscriber&), void>::value>::type>
  static std::shared_ptr<Flowable> create(F&& function) {
    return std::make_shared<Derived<F>>(std::forward<F>(function));
  }

  template <typename F,
            typename = typename std::enable_if<std::is_callable<
                F(T), typename std::result_of<F(T)>::type>::value>::type>
  auto map(F&& function);

  virtual void subscribe_on(std::unique_ptr<Subscriber> subscriber,
                            Scheduler& scheduler) = 0;

 protected:
  Flowable() = default;

 private:
  Flowable(Flowable&&) = delete;
  Flowable(const Flowable&) = delete;
  Flowable& operator=(Flowable&&) = delete;
  Flowable& operator=(const Flowable&) = delete;

  template <typename Function>
  class Derived : public Flowable {
   public:
    Derived(Function&& function)
        : function_(
              std::make_shared<Function>(std::forward<Function>(function))) {}

    void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      (new reactivestreams::DeletingSubscription<T, Function>(
           function_, std::move(subscriber)))
          ->start();
    }

    virtual void subscribe_on(std::unique_ptr<Subscriber> subscriber,
                              Scheduler& scheduler) override {
      auto subscription =
          new reactivestreams::DeletingSubscription<T, Function>(
              function_, std::move(subscriber));
      scheduler.schedule([subscription] { subscription->start(); });
    }

   private:
    const std::shared_ptr<Function> function_;
  };
};

}  // yarpl

#include "yarpl/operators/map.h"

namespace yarpl {

template <typename T>
template <typename F, typename Default>
auto Flowable<T>::map(F&& function) {
  return std::make_shared<operators::Mapper<T, F>>(std::forward<F>(function),
                                                   this->shared_from_this());
}

}  // yarpl
