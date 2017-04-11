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
template<typename U, typename D>
class Operator : public Flowable<D> {
public:
  Operator(Reference<Flowable<U>> upstream) : upstream_(upstream) {}

  virtual void subscribe(Reference<Subscriber<D>> subscriber) override {
    upstream_->subscribe(Reference<Subscription>(new Subscription(
        Reference<Flowable<D>>(this), std::move(subscriber))));
  }

protected:
  class Subscription : public ::yarpl::Subscription, public Subscriber<U> {
  public:
    Subscription(
        Reference<Flowable<D>> flowable, Reference<Subscriber<D>> subscriber)
      : flowable_(flowable), subscriber_(subscriber) {
    }

    ~Subscription() {
      subscriber_.reset();
    }

    virtual void onSubscribe(Reference<::yarpl::Subscription> subscription)
        override {
      upstream_ = subscription;
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
    }

    virtual void  request(int64_t delta) override {
      upstream_->request(delta);
    }

    virtual void cancel() override {
      upstream_->cancel();
    }

  protected:
    Reference<Flowable<D>> flowable_;
    Reference<Subscriber<D>> subscriber_;
    Reference<::yarpl::Subscription> upstream_;
  };

  Reference<Flowable<U>> upstream_;

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
  MapOperator(Reference<Flowable<U>> upstream, F&& function)
    : Operator<U, D>(upstream), function_(std::forward<F>(function)) {}

  virtual void subscribe(Reference<Subscriber<D>> subscriber) override {
    Operator<U, D>::upstream_->subscribe(Reference<Subscription>(
        new Subscription(Reference<Flowable<D>>(this), std::move(subscriber))));
  }

private:
  class Subscription : public Operator<U, D>::Subscription {
  public:
    Subscription(Reference<Flowable<D>> handle,
                 Reference<Subscriber<D>> subscriber)
      : Operator<U, D>::Subscription(handle, std::move(subscriber)) {}

    virtual void onNext(const U& value) override {
      MapOperator& map = *static_cast<MapOperator*>(
            Operator<U, D>::Subscription::flowable_.get());
      Operator<U, D>::Subscription::subscriber_->onNext(map.function_(value));
    }
  };

  F function_;
};

template<typename T>
class TakeOperator : public Operator<T, T> {
public:
  TakeOperator(Reference<Flowable<T>> upstream, int64_t limit)
    : Operator<T, T>(upstream), limit_(limit) {}

  virtual void subscribe(Reference<Subscriber<T>> subscriber) override {
    Operator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Flowable<T>>(this), limit_, std::move(subscriber))));
  }

private:
  class Subscription : public Operator<T, T>::Subscription {
  public:
    Subscription(Reference<Flowable<T>> handle, int64_t limit,
                 Reference<Subscriber<T>> subscriber)
      : Operator<T, T>::Subscription(handle, std::move(subscriber)),
        limit_(limit) {}

    virtual void onNext(const T& value) {
      if (processed_++ < limit_) {
        Operator<T, T>::Subscription::subscriber_->onNext(value);
        if (processed_ == limit_) {
          Operator<T, T>::Subscription::cancel();
          Operator<T, T>::Subscription::onComplete();
        }
      }
    }

  private:
    int64_t requested_{0};
    int64_t processed_{0};
    const int64_t limit_;
  };

  const int64_t limit_;
};

}  // yarpl
