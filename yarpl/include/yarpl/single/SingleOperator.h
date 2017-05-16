#pragma once

#include <utility>

#include "Single.h"
#include "SingleObserver.h"
#include "SingleSubscription.h"

namespace yarpl {
namespace single {
/**
 * Base (helper) class for operators.  Operators are templated on two types:
 * D (downstream) and U (upstream).  Operators are created by method calls on
 * an upstream Single, and are Singles themselves.  Multi-stage
 * pipelines
 * can be built: a Single heading a sequence of Operators.
 */
template <typename U, typename D>
class SingleOperator : public Single<D> {
 public:
  explicit SingleOperator(Reference<Single<U>> upstream)
      : upstream_(std::move(upstream)) {}

  void subscribe(Reference<SingleObserver<D>> subscriber) override {
    upstream_->subscribe(Reference<Subscription>(
        new Subscription(Reference<Single<D>>(this), std::move(subscriber))));
  }

 protected:
  ///
  /// \brief An Operator's subscription.
  ///
  /// When a pipeline chain is active, each Single has a corresponding
  /// subscription.  Except for the first one, the subscriptions are created
  /// against Operators.  Each operator subscription has two functions: as a
  /// subscriber for the previous stage; as a subscription for the next one,
  /// the user-supplied subscriber being the last of the pipeline stages.
  class Subscription : public ::yarpl::single::SingleSubscription,
                       public SingleObserver<U> {
   public:
    Subscription(
        Reference<Single<D>> flowable,
        Reference<SingleObserver<D>> subscriber)
        : flowable_(std::move(flowable)), subscriber_(std::move(subscriber)) {}

    ~Subscription() {
      subscriber_.reset();
    }

    void onSubscribe(
        Reference<::yarpl::single::SingleSubscription> subscription) override {
      upstream_ = std::move(subscription);
      subscriber_->onSubscribe(
          Reference<::yarpl::single::SingleSubscription>(this));
    }

    void onComplete() override {
      subscriber_->onComplete();
      upstream_.reset();
    }

    void onError(const std::exception_ptr error) override {
      subscriber_->onError(error);
      upstream_.reset();
    }

    void cancel() override {
      upstream_->cancel();
    }

   protected:
    /// The Single has the lambda, and other creation parameters.
    Reference<Single<D>> flowable_;

    /// This subscription controls the life-cycle of the subscriber.  The
    /// subscriber is retained as long as calls on it can be made.  (Note:
    /// the subscriber in turn maintains a reference on this subscription
    /// object until cancellation and/or completion.)
    Reference<SingleObserver<D>> subscriber_;

    /// In an active pipeline, cancel and (possibly modified) request(n)
    /// calls should be forwarded upstream.  Note that `this` is also a
    /// subscriber for the upstream stage: thus, there are cycles; all of
    /// the objects drop their references at cancel/complete.
    Reference<::yarpl::single::SingleSubscription> upstream_;
  };

  Reference<Single<U>> upstream_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_callable<F(U), D>::value>::type>
class MapOperator : public SingleOperator<U, D> {
 public:
  MapOperator(Reference<Single<U>> upstream, F&& function)
      : SingleOperator<U, D>(std::move(upstream)),
        function_(std::forward<F>(function)) {}

  void subscribe(Reference<SingleObserver<D>> subscriber) override {
    SingleOperator<U, D>::upstream_->subscribe(
        // Note: implicit cast to a reference to a subscriber.
        Reference<Subscription>(new Subscription(
            Reference<Single<D>>(this), std::move(subscriber))));
  }

 private:
  class Subscription : public SingleOperator<U, D>::Subscription {
   public:
    Subscription(
        Reference<Single<D>> single,
        Reference<SingleObserver<D>> subscriber)
        : SingleOperator<U, D>::Subscription(
              std::move(single),
              std::move(subscriber)) {}

    void onNext(U value) override {
      auto* subscriber = SingleOperator<U, D>::Subscription::subscriber_.get();
      auto* single = SingleOperator<U, D>::Subscription::flowable_.get();
      auto* map = static_cast<MapOperator*>(single);
      subscriber->onNext(map->function_(std::move(value)));
    }
  };

  F function_;
};

template <typename T, typename OnSubscribe>
class FromPublisherOperator : public Single<T> {
 public:
  explicit FromPublisherOperator(OnSubscribe&& function)
      : function_(std::move(function)) {}

  void subscribe(Reference<SingleObserver<T>> subscriber) override {
    function_(std::move(subscriber));
  }

 private:
  OnSubscribe function_;
};

} // single
} // yarpl
