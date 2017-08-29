// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include "yarpl/Observable.h"
#include "yarpl/observable/Observer.h"
#include "yarpl/observable/Subscriptions.h"

namespace yarpl {
namespace observable {

/**
 * Base (helper) class for operators.  Operators are templated on two types:
 * D (downstream) and U (upstream).  Operators are created by method calls on
 * an upstream Observable, and are Observables themselves.  Multi-stage
 * pipelines
 * can be built: a Observable heading a sequence of Operators.
 */
template <typename U, typename D, typename ThisOp>
class ObservableOperator : public Observable<D> {
 public:
  explicit ObservableOperator(Reference<Observable<U>> upstream)
      : upstream_(std::move(upstream)) {}
  using ThisOperatorT = ThisOp;

 protected:
  /// An Operator's subscription.
  ///
  /// When a pipeline chain is active, each Observable has a corresponding
  /// subscription.  Except for the first one, the subscriptions are created
  /// against Operators.  Each operator subscription has two functions: as a
  /// subscriber for the previous stage; as a subscription for the next one,
  /// the user-supplied subscriber being the last of the pipeline stages.
  class OperatorSubscription : public ::yarpl::observable::Subscription,
                               public Observer<U> {
   protected:
    OperatorSubscription(
        Reference<ThisOperatorT> observable,
        Reference<Observer<D>> observer)
        : observable_(std::move(observable)), observer_(std::move(observer)) {
      assert(observable_);
      assert(observer_);
    }

    Reference<ThisOperatorT>& getObservableOperator() {
      static_assert(
          std::is_base_of<Observable<D>, ThisOperatorT>::value,
          "Operator must be a subclass of Observable<D>");
      return observable_;
    }

    void observerOnNext(D value) {
      if (observer_) {
        observer_->onNext(std::move(value));
      }
    }

    /// Terminates both ends of an operator normally.
    void terminate() {
      terminateImpl(TerminateState::Both());
    }

    /// Terminates both ends of an operator with an error.
    void terminateErr(folly::exception_wrapper ex) {
      terminateImpl(TerminateState::Both(), std::move(ex));
    }

    // Subscription.

    void cancel() override {
      Subscription::cancel();
      terminateImpl(TerminateState::Up());
    }

    // Observer.

    void onSubscribe(
        Reference<yarpl::observable::Subscription> subscription) override {
      if (upstream_) {
        DLOG(ERROR) << "attempt to subscribe twice";
        subscription->cancel();
        return;
      }
      upstream_ = std::move(subscription);
      observer_->onSubscribe(get_ref(this));
    }

    void onComplete() override {
      terminateImpl(TerminateState::Down());
    }

    void onError(folly::exception_wrapper ex) override {
      terminateImpl(TerminateState::Down(), std::move(ex));
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

    /// Terminates an operator, sending cancel() and on{Complete,Error}()
    /// signals as necessary.
    void terminateImpl(
        TerminateState state,
        folly::exception_wrapper ex = folly::exception_wrapper{nullptr}) {
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
          if (ex) {
            observer->onError(std::move(ex));
          } else {
            observer->onComplete();
          }
        }
      }
    }

    /// The Observable has the lambda, and other creation parameters.
    Reference<ThisOperatorT> observable_;

    /// This subscription controls the life-cycle of the observer. The
    /// observer is retained as long as calls on it can be made.  (Note:
    /// the observer in turn maintains a reference on this subscription
    /// object until cancellation and/or completion.)
    Reference<Observer<D>> observer_;

    /// In an active pipeline, cancel and (possibly modified) request(n)
    /// calls should be forwarded upstream.  Note that `this` is also a
    /// observer for the upstream stage: thus, there are cycles; all of
    /// the objects drop their references at cancel/complete.
    // TODO(lehecka): this is extra field... base class has this member so
    // remove it
    Reference<::yarpl::observable::Subscription> upstream_;
  };

  Reference<Observable<U>> upstream_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_callable<F(U), D>::value>::type>
class MapOperator : public ObservableOperator<U, D, MapOperator<U, D, F>> {
  using ThisOperatorT = MapOperator<U, D, F>;
  using Super = ObservableOperator<U, D, ThisOperatorT>;

 public:
  MapOperator(Reference<Observable<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  Reference<Subscription> subscribe(Reference<Observer<D>> observer) override {
    auto subscription =
        make_ref<MapSubscription>(get_ref(this), std::move(observer));
    Super::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        subscription);
    return subscription;
  }

 private:
  class MapSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    MapSubscription(
        Reference<ThisOperatorT> observable,
        Reference<Observer<D>> observer)
        : SuperSub(std::move(observable), std::move(observer)) {}

    void onNext(U value) override {
      auto& map = SuperSub::getObservableOperator();
      SuperSub::observerOnNext(map->function_(std::move(value)));
    }
  };

  F function_;
};

template <
    typename U,
    typename F,
    typename =
        typename std::enable_if<std::is_callable<F(U), bool>::value>::type>
class FilterOperator : public ObservableOperator<U, U, FilterOperator<U, F>> {
  using ThisOperatorT = FilterOperator<U, F>;
  using Super = ObservableOperator<U, U, ThisOperatorT>;

 public:
  FilterOperator(Reference<Observable<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  Reference<Subscription> subscribe(Reference<Observer<U>> observer) override {
    auto subscription =
        make_ref<FilterSubscription>(get_ref(this), std::move(observer));
    Super::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        subscription);
    return subscription;
  }

 private:
  class FilterSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    FilterSubscription(
        Reference<ThisOperatorT> observable,
        Reference<Observer<U>> observer)
        : SuperSub(std::move(observable), std::move(observer)) {}

    void onNext(U value) override {
      auto& filter = SuperSub::getObservableOperator();
      if (filter->function_(value)) {
        SuperSub::observerOnNext(std::move(value));
      }
    }
  };

  F function_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_assignable<D, U>::value>,
    typename =
        typename std::enable_if<std::is_callable<F(D, U), D>::value>::type>
class ReduceOperator
    : public ObservableOperator<U, D, ReduceOperator<U, D, F>> {
  using ThisOperatorT = ReduceOperator<U, D, F>;
  using Super = ObservableOperator<U, D, ThisOperatorT>;

 public:
  ReduceOperator(Reference<Observable<U>> upstream, F function)
      : Super(std::move(upstream)), function_(std::move(function)) {}

  Reference<Subscription> subscribe(
      Reference<Observer<D>> subscriber) override {
    auto subscription =
        make_ref<ReduceSubscription>(get_ref(this), std::move(subscriber));
    Super::upstream_->subscribe(
        // Note: implicit cast to a reference to a subscriber.
        subscription);
    return subscription;
  }

 private:
  class ReduceSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    ReduceSubscription(
        Reference<ThisOperatorT> flowable,
        Reference<Observer<D>> subscriber)
        : SuperSub(std::move(flowable), std::move(subscriber)),
          accInitialized_(false) {}

    void onNext(U value) override {
      auto& reduce = SuperSub::getObservableOperator();
      if (accInitialized_) {
        acc_ = reduce->function_(std::move(acc_), std::move(value));
      } else {
        acc_ = std::move(value);
        accInitialized_ = true;
      }
    }

    void onComplete() override {
      if (accInitialized_) {
        SuperSub::observerOnNext(std::move(acc_));
      }
      SuperSub::onComplete();
    }

   private:
    bool accInitialized_;
    D acc_;
  };

  F function_;
};

template <typename T>
class TakeOperator : public ObservableOperator<T, T, TakeOperator<T>> {
  using ThisOperatorT = TakeOperator<T>;
  using Super = ObservableOperator<T, T, ThisOperatorT>;

 public:
  TakeOperator(Reference<Observable<T>> upstream, int64_t limit)
      : Super(std::move(upstream)), limit_(limit) {}

  Reference<Subscription> subscribe(Reference<Observer<T>> observer) override {
    auto subscription =
        make_ref<TakeSubscription>(get_ref(this), limit_, std::move(observer));
    Super::upstream_->subscribe(subscription);
    return subscription;
  }

 private:
  class TakeSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    TakeSubscription(
        Reference<ThisOperatorT> observable,
        int64_t limit,
        Reference<Observer<T>> observer)
        : SuperSub(std::move(observable), std::move(observer)), limit_(limit) {}

    void onNext(T value) override {
      if (limit_-- > 0) {
        if (pending_ > 0)
          --pending_;
        SuperSub::observerOnNext(std::move(value));
        if (limit_ == 0) {
          SuperSub::terminate();
        }
      }
    }

   private:
    int64_t pending_{0};
    int64_t limit_;
  };

  const int64_t limit_;
};

template <typename T>
class SkipOperator : public ObservableOperator<T, T, SkipOperator<T>> {
  using ThisOperatorT = SkipOperator<T>;
  using Super = ObservableOperator<T, T, ThisOperatorT>;

 public:
  SkipOperator(Reference<Observable<T>> upstream, int64_t offset)
      : Super(std::move(upstream)), offset_(offset) {}

  Reference<Subscription> subscribe(Reference<Observer<T>> observer) override {
    auto subscription =
        make_ref<SkipSubscription>(get_ref(this), offset_, std::move(observer));
    Super::upstream_->subscribe(subscription);
    return subscription;
  }

 private:
  class SkipSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    SkipSubscription(
        Reference<ThisOperatorT> observable,
        int64_t offset,
        Reference<Observer<T>> observer)
        : SuperSub(std::move(observable), std::move(observer)),
          offset_(offset) {}

    void onNext(T value) override {
      if (offset_ <= 0) {
        SuperSub::observerOnNext(std::move(value));
      } else {
        --offset_;
      }
    }

   private:
    int64_t offset_;
  };

  const int64_t offset_;
};

template <typename T>
class IgnoreElementsOperator
    : public ObservableOperator<T, T, IgnoreElementsOperator<T>> {
  using ThisOperatorT = IgnoreElementsOperator<T>;
  using Super = ObservableOperator<T, T, ThisOperatorT>;

 public:
  explicit IgnoreElementsOperator(Reference<Observable<T>> upstream)
      : Super(std::move(upstream)) {}

  Reference<Subscription> subscribe(Reference<Observer<T>> observer) override {
    auto subscription = make_ref<IgnoreElementsSubscription>(
        get_ref(this), std::move(observer));
    Super::upstream_->subscribe(subscription);
    return subscription;
  }

 private:
  class IgnoreElementsSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    IgnoreElementsSubscription(
        Reference<ThisOperatorT> observable,
        Reference<Observer<T>> observer)
        : SuperSub(std::move(observable), std::move(observer)) {}

    void onNext(T) override {}
  };
};

template <typename T>
class SubscribeOnOperator
    : public ObservableOperator<T, T, SubscribeOnOperator<T>> {
  using ThisOperatorT = SubscribeOnOperator<T>;
  using Super = ObservableOperator<T, T, ThisOperatorT>;

 public:
  SubscribeOnOperator(Reference<Observable<T>> upstream, Scheduler& scheduler)
      : Super(std::move(upstream)), worker_(scheduler.createWorker()) {}

  Reference<Subscription> subscribe(Reference<Observer<T>> observer) override {
    auto subscription = make_ref<SubscribeOnSubscription>(
        get_ref(this), std::move(worker_), std::move(observer));
    Super::upstream_->subscribe(subscription);
    return subscription;
  }

 private:
  class SubscribeOnSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    SubscribeOnSubscription(
        Reference<ThisOperatorT> observable,
        std::unique_ptr<Worker> worker,
        Reference<Observer<T>> observer)
        : SuperSub(std::move(observable), std::move(observer)),
          worker_(std::move(worker)) {}

    void cancel() override {
      worker_->schedule([this] { this->callSuperCancel(); });
    }

    void onNext(T value) override {
      SuperSub::observerOnNext(std::move(value));
    }

   private:
    // Trampoline to call superclass method; gcc bug 58972.
    void callSuperCancel() {
      SuperSub::cancel();
    }

    std::unique_ptr<Worker> worker_;
  };

  std::unique_ptr<Worker> worker_;
};

template <typename T, typename OnSubscribe>
class FromPublisherOperator : public Observable<T> {
 public:
  explicit FromPublisherOperator(OnSubscribe function)
      : function_(std::move(function)) {}

 private:
  class PublisherObserver : public Observer<T> {
   public:
    PublisherObserver(
        Reference<Observer<T>> inner,
        Reference<Subscription> subscription)
        : inner_(std::move(inner)) {
      Observer<T>::onSubscribe(std::move(subscription));
    }

    void onSubscribe(Reference<Subscription>) override {
      DLOG(ERROR) << "not allowed to call";
      CHECK(false);
    }

    void onComplete() override {
      inner_->onComplete();
      Observer<T>::onComplete();
    }

    void onError(folly::exception_wrapper ex) override {
      inner_->onError(std::move(ex));
      Observer<T>::onError(folly::exception_wrapper());
    }

    void onNext(T t) override {
      inner_->onNext(std::move(t));
    }

   private:
    Reference<Observer<T>> inner_;
  };

 public:
  Reference<Subscription> subscribe(Reference<Observer<T>> observer) override {
    auto subscription = Subscriptions::create();
    observer->onSubscribe(subscription);

    if (!subscription->isCancelled()) {
      function_(make_ref<PublisherObserver>(std::move(observer), subscription));
    }
    return subscription;
  }

 private:
  OnSubscribe function_;
};
}
}
