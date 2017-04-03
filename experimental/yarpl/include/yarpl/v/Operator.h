#pragma once

#include <iostream>
#include <utility>

#include "Flowable.h"
#include "Subscriber.h"
#include "Subscription.h"

namespace yarpl {

/**
 * Base (helper) class for operators.  Operators are templated on two types:
 * D and U.
 */
template<typename U, typename D>
class Operator : public Flowable<D> {
public:
  Operator(typename Flowable<U>::Handle upstream) : upstream_(upstream) {}

  virtual void subscribe(std::unique_ptr<Subscriber<D>> subscriber) override {
    upstream_->subscribe(std::make_unique<Subscription>(
        typename Flowable<D>::Handle(this), std::move(subscriber)));
  }

protected:
  class Subscription : public ::yarpl::Subscription, public Subscriber<U> {
  public:
    Subscription(typename Flowable<D>::Handle flowable,
                 std::unique_ptr<Subscriber<D>> subscriber)
      : flowable_(flowable), subscriber_(std::move(subscriber)) {
    }

    virtual void onSubscribe(::yarpl::Subscription::Handle subscription)
        override {
      upstream_ = subscription;
      subscriber_->onSubscribe(::yarpl::Subscription::Handle(this));
    }

    virtual void onComplete() override {
      subscriber_->onComplete();
    }

    virtual void onError(const std::exception_ptr error) override {
      subscriber_->onError(error);
    }

    virtual void onNext(const U&) override {
    }

    virtual void  request(int64_t delta) override {
      upstream_->request(delta);
    }

    virtual void cancel() override {
      upstream_->cancel();
    }

  protected:
    typename Flowable<D>::Handle flowable_;
    std::unique_ptr<Subscriber<D>> subscriber_;
    ::yarpl::Subscription::Handle upstream_;
  };

  typename Flowable<U>::Handle upstream_;

private:
  // TODO(vjn): fix: this shouldn't be needed.
  virtual std::tuple<int64_t, bool> emit(Subscriber<D>&, int64_t) override {
    return std::make_tuple(0, false);
  }
};

template<typename U, typename D, typename F, typename = typename
         std::enable_if<std::is_callable<F(U), D>::value>::type>
class MapOperator : public Operator<U, D> {
public:
  MapOperator(typename Flowable<U>::Handle upstream, F&& function)
    : Operator<U, D>(upstream), function_(std::forward<F>(function)) {}

  virtual void subscribe(std::unique_ptr<Subscriber<D>> subscriber) override {
    Operator<U, D>::upstream_->subscribe(
        std::make_unique<Subscription>(
            typename Flowable<D>::Handle(this), std::move(subscriber)));
  }

protected:
  class Subscription : public Operator<U, D>::Subscription {
  public:
    Subscription(typename Flowable<D>::Handle handle,
                 std::unique_ptr<Subscriber<D>> subscriber)
      : Operator<U, D>::Subscription(handle, std::move(subscriber)) {}

    virtual void onNext(const U& value) override {
      MapOperator& map = *static_cast<MapOperator*>(
            Operator<U, D>::Subscription::flowable_.get());
      Operator<U, D>::Subscription::subscriber_->onNext(map.function_(value));
    }
  };

private:
  F function_;
};

}  // yarpl
