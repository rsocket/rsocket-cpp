// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Try.h>
#include <folly/functional/Invoke.h>

#include <utility>

#include "yarpl/single/Single.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscriptions.h"

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
  template <typename Operator>
  class Subscription : public ::yarpl::single::SingleSubscription,
                       public SingleObserver<U> {
   protected:
    Subscription(
        Reference<Operator> single,
        Reference<SingleObserver<D>> observer)
        : single_(std::move(single)), observer_(std::move(observer)) {}

    ~Subscription() {
      observer_.reset();
    }

    void observerOnSuccess(D value) {
      terminateImpl(TerminateState::Down(), folly::Try<D>{std::move(value)});
    }

    void observerOnError(folly::exception_wrapper ew) {
      terminateImpl(TerminateState::Down(), folly::Try<D>{std::move(ew)});
    }

    Reference<Operator> getOperator() {
      return single_;
    }

    void terminateErr(folly::exception_wrapper ew) {
      terminateImpl(TerminateState::Both(), std::move(ew));
    }

    // SingleSubscription.

    void cancel() override {
      terminateImpl(TerminateState::Up(), folly::Try<D>{});
    }

    // Subscriber.

    void onSubscribe(
        Reference<yarpl::single::SingleSubscription> subscription) override {
      upstream_ = std::move(subscription);
      observer_->onSubscribe(this->ref_from_this(this));
    }

    void onError(folly::exception_wrapper ew) override {
      terminateImpl(TerminateState::Down(), folly::Try<D>{std::move(ew)});
    }

   private:
    struct TerminateState {
      TerminateState(bool u, bool d) : up{u}, down{d} {}

      static TerminateState Down() {
        return TerminateState{false, true};
      }

      static TerminateState Up() {
        return TerminateState{true, false};
      }

      static TerminateState Both() {
        return TerminateState{true, true};
      }

      const bool up{false};
      const bool down{false};
    };

    bool isTerminated() const {
      return !upstream_ && !observer_;
    }

    void terminateImpl(TerminateState state, folly::Try<D> maybe) {
      if (isTerminated()) {
        return;
      }

      if (auto upstream = std::move(upstream_)) {
        if (state.up) {
          upstream->cancel();
        }
      }

      if (auto observer = std::move(observer_)) {
        if (state.down) {
          if (maybe.hasValue()) {
            observer->onSuccess(std::move(maybe).value());
          } else {
            observer->onError(std::move(maybe).exception());
          }
        }
      }
    }

    /// The Single has the lambda, and other creation parameters.
    Reference<Operator> single_;

    /// This subscription controls the life-cycle of the observer.  The
    /// observer is retained as long as calls on it can be made.  (Note:
    /// the observer in turn maintains a reference on this subscription
    /// object until cancellation and/or completion.)
    Reference<SingleObserver<D>> observer_;

    /// In an active pipeline, cancel and (possibly modified) request(n)
    /// calls should be forwarded upstream.  Note that `this` is also a
    /// observer for the upstream stage: thus, there are cycles; all of
    /// the objects drop their references at cancel/complete.
    Reference<yarpl::single::SingleSubscription> upstream_;
  };

  Reference<Single<U>> upstream_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<folly::is_invocable_r<D, F, U>::value>::type>
class MapOperator : public SingleOperator<U, D> {
  using ThisOperatorT = MapOperator<U, D, F>;
  using Super = SingleOperator<U, D>;
  using OperatorSubscription =
      typename Super::template Subscription<ThisOperatorT>;

 public:
  MapOperator(Reference<Single<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  void subscribe(Reference<SingleObserver<D>> observer) override {
    Super::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        make_ref<MapSubscription>(
            this->ref_from_this(this), std::move(observer)));
  }

 private:
  class MapSubscription : public OperatorSubscription {
   public:
    MapSubscription(
        Reference<ThisOperatorT> single,
        Reference<SingleObserver<D>> observer)
        : OperatorSubscription(std::move(single), std::move(observer)) {}

    void onSuccess(U value) override {
      try {
        auto map_operator = this->getOperator();
        this->observerOnSuccess(map_operator->function_(std::move(value)));
      } catch (const std::exception& exn) {
        folly::exception_wrapper ew{std::current_exception(), exn};
        this->observerOnError(std::move(ew));
      }
    }
  };

  F function_;
};

template <typename T, typename OnSubscribe>
class FromPublisherOperator : public Single<T> {
 public:
  explicit FromPublisherOperator(OnSubscribe function)
      : function_(std::move(function)) {}

  void subscribe(Reference<SingleObserver<T>> observer) override {
    function_(std::move(observer));
  }

 private:
  OnSubscribe function_;
};

template <typename OnSubscribe>
class SingleVoidFromPublisherOperator : public Single<void> {
 public:
  explicit SingleVoidFromPublisherOperator(OnSubscribe&& function)
      : function_(std::move(function)) {}

  void subscribe(Reference<SingleObserverBase<void>> observer) override {
    function_(std::move(observer));
  }

 private:
  OnSubscribe function_;
};

} // single
} // yarpl
