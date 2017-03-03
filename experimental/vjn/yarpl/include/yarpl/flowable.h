#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <reactivestreams/publisher.h>
#include <reactivestreams/subscription.h>
#include <reactivestreams/type_traits.h>

#include "yarpl/scheduler.h"

namespace yarpl {

template<typename T>
class Flowable : public reactivestreams::Publisher<T>,
    public std::enable_shared_from_this<Flowable<T>> {
  using Subscriber = reactivestreams::Subscriber<T>;
  using Subscription = reactivestreams::Subscription<T>;

public:
  template<typename F, typename = typename std::enable_if<
      std::is_callable<F(Subscriber&), void>::value>::type>
  static std::shared_ptr<Flowable> create(F&& function) {
    return std::shared_ptr<Flowable>(
        new Derived<F>(std::forward<F>(function)));
  }

  template<typename F, typename = typename std::enable_if<
      std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::type>
  auto map(F&& function) {
    return std::shared_ptr<Flowable<typename std::result_of<F(T)>::type>>(
        new Mapper<F>(std::forward<F>(function), this->shared_from_this()));
  }

  virtual void subscribe_on(std::unique_ptr<Subscriber> subscriber,
      Scheduler& scheduler) = 0;

protected:
  Flowable() = default;

private:
  Flowable(Flowable&&) = delete;
  Flowable(const Flowable&) = delete;
  Flowable& operator=(Flowable&&) = delete;
  Flowable& operator=(const Flowable &) = delete;

  template<typename Function>
  class Derived : public Flowable {
  public:
    Derived(Function&& function)
        : function_(std::make_shared<Function>(
            std::forward<Function>(function))) {}

    void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      (new reactivestreams::DeletingSubscription<T, Function>(
          function_, std::move(subscriber)))->start();
    }

    virtual void subscribe_on(std::unique_ptr<Subscriber> subscriber,
        Scheduler& scheduler) override {
      auto subscription = new reactivestreams::DeletingSubscription<
          T, Function>(function_, std::move(subscriber));
      scheduler.schedule([subscription] {
        subscription->start();
      });
    }

  private:
    const std::shared_ptr<Function> function_;
  };

  template<typename Transform>
  class Mapper : public Flowable, public Subscriber {
  public:
    Mapper(Transform&& transform, std::shared_ptr<Flowable<T>> upstream)
        : forwarder_(std::unique_ptr<Forwarder>(
            new Forwarder(std::forward<Transform>(transform)))),
          upstream_(upstream) {}

    void subscribe(std::unique_ptr<Subscriber> subscriber) override {
      // TODO(vjn): check that subscriber_.get() == nullptr?
      forwarder_->set_subscriber(std::move(subscriber));
      upstream_->subscribe(std::move(forwarder_));
    }

    virtual void subscribe_on(std::unique_ptr<Subscriber> subscriber,
        Scheduler& scheduler) override {
    }
  private:
    class Forwarder : public Subscriber {
    public:
      Forwarder(Transform&& transform)
          : transform_(std::forward<Transform>(transform)) {}

      void set_subscriber(std::unique_ptr<Subscriber> subscriber) {
        subscriber_ = std::move(subscriber);
      }

      virtual void on_next(const T& value) override {
        subscriber_->on_next(transform_(value));
      }

      virtual void on_error(const std::exception& error) override {
        subscriber_->on_error(error);
      }

      virtual void on_complete() override {
        subscriber_->on_complete();
      }

      virtual void on_subscribe(Subscription* subscription) override {
        subscriber_->on_subscribe(subscription);
      }

    private:
      Transform transform_;
      std::unique_ptr<Subscriber> subscriber_;
    };

    std::unique_ptr<Forwarder> forwarder_;
    const std::shared_ptr<Flowable<T>> upstream_;
  };
};

}  // yarpl
