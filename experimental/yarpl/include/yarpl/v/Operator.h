#pragma once

#include <utility>

#include "Flowable.h"
#include "Subscriber.h"
#include "Subscription.h"

namespace yarpl {

/**
 * Base (helper) class for operators.  Operators are templated on two types:
 * D and U.
 *
 */
template <typename U, typename D>
class Operator : public Flowable<D> {
 public:
  Operator(Reference<Flowable<U>> upstream) : upstream_(std::move(upstream)) {}

  virtual void subscribe(Reference<Subscriber<D>> subscriber) override {
    upstream_->subscribe(Reference<Subscription>(
        new Subscription(Reference<Flowable<D>>(this), std::move(subscriber))));
  }

 protected:
  class Subscription : public ::yarpl::Subscription, public Subscriber<U> {
   public:
    Subscription(
        Reference<Flowable<D>> flowable,
        Reference<Subscriber<D>> subscriber)
        : flowable_(std::move(flowable)), subscriber_(std::move(subscriber)) {}

    ~Subscription() {
      subscriber_.reset();
    }

    virtual void onSubscribe(
        Reference<::yarpl::Subscription> subscription) override {
      upstream_ = std::move(subscription);
      subscriber_->onSubscribe(Reference<::yarpl::Subscription>(this));
    }

    virtual void onComplete() override {
      subscriber_->onComplete();
      upstream_.reset();
      release();
    }

    virtual void onError(const std::exception_ptr error) override {
      subscriber_->onError(error);
      upstream_.reset();
      release();
    }

    virtual void request(int64_t delta) override {
      upstream_->request(delta);
    }

    virtual void cancel() override {
      upstream_->cancel();
      release();
    }

   protected:
    Reference<Flowable<D>> flowable_;
    Reference<Subscriber<D>> subscriber_;
    Reference<::yarpl::Subscription> upstream_;
  };

  Reference<Flowable<U>> upstream_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_callable<F(U), D>::value>::type>
class MapOperator : public Operator<U, D> {
 public:
  MapOperator(Reference<Flowable<U>> upstream, F&& function)
      : Operator<U, D>(std::move(upstream)),
        function_(std::forward<F>(function)) {}

  virtual void subscribe(Reference<Subscriber<D>> subscriber) override {
    Operator<U, D>::upstream_->subscribe(
        // Note: implicit cast to a reference to a subscriber.
        Reference<Subscription>(new Subscription(
            Reference<Flowable<D>>(this), std::move(subscriber))));
  }

 private:
  class Subscription : public Operator<U, D>::Subscription {
   public:
    Subscription(
        Reference<Flowable<D>> flowable,
        Reference<Subscriber<D>> subscriber)
        : Operator<U, D>::Subscription(
              std::move(flowable),
              std::move(subscriber)) {}

    virtual void onNext(const U& value) override {
      auto* subscriber = Operator<U, D>::Subscription::subscriber_.get();
      auto* flowable = Operator<U, D>::Subscription::flowable_.get();
      auto* map = static_cast<MapOperator*>(flowable);
      subscriber->onNext(map->function_(value));
    }
  };

  F function_;
};

template <typename T>
class TakeOperator : public Operator<T, T> {
 public:
  TakeOperator(Reference<Flowable<T>> upstream, int64_t limit)
      : Operator<T, T>(std::move(upstream)), limit_(limit) {}

  virtual void subscribe(Reference<Subscriber<T>> subscriber) override {
    Operator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Flowable<T>>(this), limit_, std::move(subscriber))));
  }

 private:
  class Subscription : public Operator<T, T>::Subscription {
   public:
    Subscription(
        Reference<Flowable<T>> flowable,
        int64_t limit,
        Reference<Subscriber<T>> subscriber)
        : Operator<T, T>::Subscription(
              std::move(flowable),
              std::move(subscriber)),
          limit_(limit) {}

    virtual void onNext(const T& value) {
      if (limit_-- > 0) {
        if (pending_ > 0)
          --pending_;
        Operator<T, T>::Subscription::subscriber_->onNext(value);
        if (limit_ == 0) {
          Operator<T, T>::Subscription::cancel();
          Operator<T, T>::Subscription::onComplete();
        }
      }
    }

    virtual void request(int64_t delta) {
      delta = std::min(delta, limit_ - pending_);
      if (delta > 0) {
        pending_ += delta;
        Operator<T, T>::Subscription::upstream_->request(delta);
      }
    }

   private:
    int64_t pending_{0};
    int64_t limit_;
  };

  const int64_t limit_;
};

} // yarpl
