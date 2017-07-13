// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include "yarpl/single/Single.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscription.h"

namespace yarpl {
namespace single {
/**
 * Base (helper) class for operators.  Operators are templated on two types:
 * D (downstream) and U (upstream).  Operators are created by method calls on
 * an upstream Single, and are Observables themselves.  Multi-stage
 * pipelines
 * can be built: a Single heading a sequence of Operators.
 */
template <typename U, typename D>
class SingleOperator : public Single<D> {
 public:
  explicit SingleOperator(Reference<Single<U>> upstream)
      : upstream_(std::move(upstream)) {}

 protected:
  ///
  /// \brief An Operator's subscription.
  ///
  /// When a pipeline chain is active, each Single has a corresponding
  /// subscription.  Except for the first one, the subscriptions are created
  /// against Operators.  Each operator subscription has two functions: as a
  /// observer for the previous stage; as a subscription for the next one,
  /// the user-supplied observer being the last of the pipeline stages.
  class Subscription : public ::yarpl::single::SingleSubscription,
                       public SingleObserver<U> {
   protected:
    Subscription(
        Reference<Single<D>> single,
        Reference<SingleObserver<D>> observer)
        : single_(std::move(single)), observer_(std::move(observer)) {}

    ~Subscription() {
      observer_.reset();
    }

    void observerOnSuccess(D value) {
      observer_->onSuccess(std::move(value));
      upstreamSubscription_.reset(); // should break the cycle to this
    }

    template<typename TOperator>
    TOperator* getObservableAs() {
      return static_cast<TOperator*>(single_.get());
    }

   private:
    void onSubscribe(
        Reference<::yarpl::single::SingleSubscription> subscription) override {
      upstreamSubscription_ = std::move(subscription);
      observer_->onSubscribe(
          Reference<::yarpl::single::SingleSubscription>(this));
    }

    void onError(std::exception_ptr error) override {
      observer_->onError(error);
      upstreamSubscription_.reset(); // should break the cycle to this
    }

    void cancel() override {
      upstreamSubscription_->cancel();
      observer_.reset(); // breaking the cycle
    }

    /// The Single has the lambda, and other creation parameters.
    Reference<Single<D>> single_;

    /// This subscription controls the life-cycle of the observer.  The
    /// observer is retained as long as calls on it can be made.  (Note:
    /// the observer in turn maintains a reference on this subscription
    /// object until cancellation and/or completion.)
    Reference<SingleObserver<D>> observer_;

    /// In an active pipeline, cancel and (possibly modified) request(n)
    /// calls should be forwarded upstream.  Note that `this` is also a
    /// observer for the upstream stage: thus, there are cycles; all of
    /// the objects drop their references at cancel/complete.
    Reference<::yarpl::single::SingleSubscription> upstreamSubscription_;
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

  void subscribe(Reference<SingleObserver<D>> observer) override {
    SingleOperator<U, D>::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        Reference<Subscription>(new Subscription(
            Reference<Single<D>>(this), std::move(observer))));
  }

 private:
  class Subscription : public SingleOperator<U, D>::Subscription {
    using Super = typename SingleOperator<U, D>::Subscription;
   public:
    Subscription(
        Reference<Single<D>> single,
        Reference<SingleObserver<D>> observer)
        : SingleOperator<U, D>::Subscription(
              std::move(single),
              std::move(observer)) {}

    void onSuccess(U value) override {
      auto* map = Super::template getObservableAs<MapOperator>();
      Super::observerOnSuccess(map->function_(std::move(value)));
    }
  };

  F function_;
};

template <typename T, typename OnSubscribe>
class FromPublisherOperator : public Single<T> {
 public:
  explicit FromPublisherOperator(OnSubscribe&& function)
      : function_(std::move(function)) {}

  void subscribe(Reference<SingleObserver<T>> observer) override {
    function_(std::move(observer));
  }

 private:
  OnSubscribe function_;
};

} // observable
} // yarpl
