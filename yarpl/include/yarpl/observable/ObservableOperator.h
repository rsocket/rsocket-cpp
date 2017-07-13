// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include "yarpl/Observable.h"
#include "yarpl/observable/Observer.h"
#include "yarpl/observable/Subscription.h"

namespace yarpl {
namespace observable {
/**
 * Base (helper) class for operators.  Operators are templated on two types:
 * D (downstream) and U (upstream).  Operators are created by method calls on
 * an upstream Observable, and are Observables themselves.  Multi-stage
 * pipelines
 * can be built: a Observable heading a sequence of Operators.
 */
template <typename U, typename D>
class ObservableOperator : public Observable<D> {
 public:
  explicit ObservableOperator(Reference<Observable<U>> upstream)
      : upstream_(std::move(upstream)) {}

 protected:
  ///
  /// \brief An Operator's subscription.
  ///
  /// When a pipeline chain is active, each Observable has a corresponding
  /// subscription.  Except for the first one, the subscriptions are created
  /// against Operators.  Each operator subscription has two functions: as a
  /// subscriber for the previous stage; as a subscription for the next one,
  /// the user-supplied subscriber being the last of the pipeline stages.
  class Subscription : public ::yarpl::observable::Subscription,
                       public Observer<U> {
   protected:
    Subscription(
        Reference<Observable<D>> observable,
        Reference<Observer<D>> observer)
        : observable_(std::move(observable)), observer_(std::move(observer)) {
      assert(observable_);
      assert(observer_);
    }

    ~Subscription() {
      observer_.reset();
    }

    template<typename TOperator>
    TOperator* getObservableAs() {
      return static_cast<TOperator*>(observable_.get());
    }

    // this method should be used to terminate the operators
    void terminate() {
      observer_->onComplete(); // should break the cycle to this
      upstream_->cancel(); // should break the cycle to this
    }

    void observerOnNext(D value) {
      observer_->onNext(std::move(value));
    }

   protected:
    void onComplete() override {
      observer_->onComplete();
      upstream_.reset(); // breaking the cycle
    }

  private:
    void onSubscribe(
        Reference<::yarpl::observable::Subscription> subscription) override {
      upstream_ = std::move(subscription);
      observer_->onSubscribe(
          Reference<::yarpl::observable::Subscription>(this));
    }

    void onError(std::exception_ptr error) override {
      observer_->onError(error);
      upstream_.reset(); // breaking the cycle
    }

    void cancel() override {
      upstream_->cancel();
      observer_.reset(); // breaking the cycle
    }

    /// The Observable has the lambda, and other creation parameters.
    Reference<Observable<D>> observable_;

    /// This subscription controls the life-cycle of the observer. The
    /// observer is retained as long as calls on it can be made.  (Note:
    /// the observer in turn maintains a reference on this subscription
    /// object until cancellation and/or completion.)
    Reference<Observer<D>> observer_;

    /// In an active pipeline, cancel and (possibly modified) request(n)
    /// calls should be forwarded upstream.  Note that `this` is also a
    /// observer for the upstream stage: thus, there are cycles; all of
    /// the objects drop their references at cancel/complete.
    Reference<::yarpl::observable::Subscription> upstream_;
  };

  Reference<Observable<U>> upstream_;
};

template <
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_callable<F(U), D>::value>::type>
class MapOperator : public ObservableOperator<U, D> {
 public:
  MapOperator(Reference<Observable<U>> upstream, F&& function)
      : ObservableOperator<U, D>(std::move(upstream)),
        function_(std::forward<F>(function)) {}

  void subscribe(Reference<Observer<D>> observer) override {
    ObservableOperator<U, D>::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        Reference<Subscription>(new Subscription(
            Reference<Observable<D>>(this), std::move(observer))));
  }

 private:
  class Subscription : public ObservableOperator<U, D>::Subscription {
    using Super = typename ObservableOperator<U, D>::Subscription;
   public:
    Subscription(
        Reference<Observable<D>> observable,
        Reference<Observer<D>> observer)
        : Super(
              std::move(observable),
              std::move(observer)) {}

    void onNext(U value) override {
      auto* map = Super::template getObservableAs<MapOperator>();
      Super::observerOnNext(map->function_(std::move(value)));
    }
  };

  F function_;
};

template <
    typename U,
    typename F,
    typename =
        typename std::enable_if<std::is_callable<F(U), bool>::value>::type>
class FilterOperator : public ObservableOperator<U, U> {
 public:
  FilterOperator(Reference<Observable<U>> upstream, F&& function)
      : ObservableOperator<U, U>(std::move(upstream)),
        function_(std::forward<F>(function)) {}

  void subscribe(Reference<Observer<U>> observer) override {
    ObservableOperator<U, U>::upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        Reference<Subscription>(new Subscription(
            Reference<Observable<U>>(this), std::move(observer))));
  }

 private:
  class Subscription : public ObservableOperator<U, U>::Subscription {
    using Super = typename ObservableOperator<U, U>::Subscription;
   public:
    Subscription(
        Reference<Observable<U>> observable,
        Reference<Observer<U>> observer)
        : Super(
              std::move(observable),
              std::move(observer)) {}

    void onNext(U value) override {
      auto* filter = Super::template getObservableAs<FilterOperator>();
      if (filter->function_(value)) {
        Super::observerOnNext(std::move(value));
      }
    }
  };

  F function_;
};

template<
    typename U,
    typename D,
    typename F,
    typename = typename std::enable_if<std::is_assignable<D, U>::value>,
    typename = typename std::enable_if<std::is_callable<F(D, U), D>::value>::type>
class ReduceOperator : public ObservableOperator<U, D> {
public:
  ReduceOperator(Reference<Observable<U>> upstream, F &&function)
      : ObservableOperator<U, D>(std::move(upstream)),
        function_(std::forward<F>(function)) {}

  void subscribe(Reference<Observer<D>> subscriber) override {
    ObservableOperator<U, D>::upstream_->subscribe(
        // Note: implicit cast to a reference to a subscriber.
        Reference<Subscription>(new Subscription(
            Reference<Observable<D>>(this), std::move(subscriber))));
  }

private:
  class Subscription : public ObservableOperator<U, D>::Subscription {
    using Super = typename ObservableOperator<U, D>::Subscription;

  public:
    Subscription(
        Reference <Observable<D>> flowable,
        Reference <Observer<D>> subscriber)
        : Super(
        std::move(flowable),
        std::move(subscriber)),
          accInitialized_(false) {}

    void onNext(U value) override {
      auto* reduce = Super::template getObservableAs<ReduceOperator>();
      if (accInitialized_) {
        acc_ = reduce->function_(std::move(acc_), std::move(value));
      } else {
        acc_ = std::move(value);
        accInitialized_ = true;
      }
    }

    void onComplete() override {
      if (accInitialized_) {
        Super::observerOnNext(std::move(acc_));
      }
      Super::onComplete();
    }

  private:
    bool accInitialized_;
    D acc_;
  };

  F function_;
};

template <typename T>
class TakeOperator : public ObservableOperator<T, T> {
 public:
  TakeOperator(Reference<Observable<T>> upstream, int64_t limit)
      : ObservableOperator<T, T>(std::move(upstream)), limit_(limit) {}

  void subscribe(Reference<Observer<T>> observer) override {
    ObservableOperator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Observable<T>>(this), limit_, std::move(observer))));
  }

 private:
  class Subscription : public ObservableOperator<T, T>::Subscription {
    using Super = typename ObservableOperator<T,T>::Subscription;
   public:
    Subscription(
        Reference<Observable<T>> observable,
        int64_t limit,
        Reference<Observer<T>> observer)
        : Super(
              std::move(observable),
              std::move(observer)),
          limit_(limit) {}

    void onNext(T value) override {
      if (limit_-- > 0) {
        if (pending_ > 0)
          --pending_;
        Super::observerOnNext(
            std::move(value));
        if (limit_ == 0) {
          Super::terminate();
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
class SkipOperator : public ObservableOperator<T, T> {
 public:
  SkipOperator(Reference<Observable<T>> upstream, int64_t offset)
      : ObservableOperator<T, T>(std::move(upstream)), offset_(offset) {}

  void subscribe(Reference<Observer<T>> observer) override {
    ObservableOperator<T, T>::upstream_->subscribe(
      make_ref<Subscription>(
          Reference<Observable<T>>(this), offset_, std::move(observer)));
  }

 private:
  class Subscription : public ObservableOperator<T, T>::Subscription {
    using Super = typename ObservableOperator<T,T>::Subscription;
   public:
    Subscription(
       Reference<Observable<T>> observable,
       int64_t offset,
       Reference<Observer<T>> observer)
       : Super(std::move(observable), std::move(observer)),
       offset_(offset) {}

    void onNext(T value) override {
      if (offset_ <= 0) {
        Super::observerOnNext(
            std::move(value));
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
class IgnoreElementsOperator : public ObservableOperator<T, T> {
 public:
  explicit IgnoreElementsOperator(Reference<Observable<T>> upstream)
      : ObservableOperator<T, T>(std::move(upstream)) {}

  void subscribe(Reference<Observer<T>> observer) override {
    ObservableOperator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Observable<T>>(this), std::move(observer))));
  }

 private:
  class Subscription : public ObservableOperator<T, T>::Subscription {
    using Super = typename ObservableOperator<T,T>::Subscription;
   public:
    Subscription(
        Reference<Observable<T>> observable,
        Reference<Observer<T>> observer)
        : ObservableOperator<T, T>::Subscription(
              std::move(observable),
              std::move(observer)) {}

    void onNext(T) override {}
  };
};

template <typename T>
class SubscribeOnOperator : public ObservableOperator<T, T> {
 public:
  SubscribeOnOperator(Reference<Observable<T>> upstream, Scheduler& scheduler)
      : ObservableOperator<T, T>(std::move(upstream)),
        worker_(scheduler.createWorker()) {}

  void subscribe(Reference<Observer<T>> observer) override {
    ObservableOperator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Observable<T>>(this),
            std::move(worker_),
            std::move(observer))));
  }

 private:
  class Subscription : public ObservableOperator<T, T>::Subscription {
   public:
    Subscription(
        Reference<Observable<T>> observable,
        std::unique_ptr<Worker> worker,
        Reference<Observer<T>> observer)
        : ObservableOperator<T, T>::Subscription(
              std::move(observable),
              std::move(observer)),
          worker_(std::move(worker)) {}

    void cancel() override {
      worker_->schedule([this] { this->callSuperCancel(); });
    }

    void onNext(T value) override {
      auto* observer =
          ObservableOperator<T, T>::Subscription::observer_.get();
      observer->onNext(std::move(value));
    }

   private:
    // Trampoline to call superclass method; gcc bug 58972.
    void callSuperCancel() {
      ObservableOperator<T, T>::Subscription::cancel();
    }

    std::unique_ptr<Worker> worker_;
  };

  std::unique_ptr<Worker> worker_;
};

template <typename T, typename OnSubscribe>
class FromPublisherOperator : public Observable<T> {
 public:
  explicit FromPublisherOperator(OnSubscribe&& function)
      : function_(std::move(function)) {}

  void subscribe(Reference<Observer<T>> observer) override {
    function_(std::move(observer));
  }

 private:
  OnSubscribe function_;
};

} // observable
} // yarpl
