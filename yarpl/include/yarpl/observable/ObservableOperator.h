#pragma once

#include <utility>

#include "../Observable.h"
#include "Observer.h"
#include "Subscription.h"

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

  void subscribe(Reference<Observer<D>> subscriber) override {
    upstream_->subscribe(Reference<Subscription>(new Subscription(
        Reference<Observable<D>>(this), std::move(subscriber))));
  }

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
   public:
    Subscription(
        Reference<Observable<D>> flowable,
        Reference<Observer<D>> subscriber)
        : flowable_(std::move(flowable)), subscriber_(std::move(subscriber)) {}

    ~Subscription() {
      subscriber_.reset();
    }

    void onSubscribe(
        Reference<::yarpl::observable::Subscription> subscription) override {
      upstream_ = std::move(subscription);
      subscriber_->onSubscribe(
          Reference<::yarpl::observable::Subscription>(this));
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
    /// The Observable has the lambda, and other creation parameters.
    Reference<Observable<D>> flowable_;

    /// This subscription controls the life-cycle of the subscriber.  The
    /// subscriber is retained as long as calls on it can be made.  (Note:
    /// the subscriber in turn maintains a reference on this subscription
    /// object until cancellation and/or completion.)
    Reference<Observer<D>> subscriber_;

    /// In an active pipeline, cancel and (possibly modified) request(n)
    /// calls should be forwarded upstream.  Note that `this` is also a
    /// subscriber for the upstream stage: thus, there are cycles; all of
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

  void subscribe(Reference<Observer<D>> subscriber) override {
    ObservableOperator<U, D>::upstream_->subscribe(
        // Note: implicit cast to a reference to a subscriber.
        Reference<Subscription>(new Subscription(
            Reference<Observable<D>>(this), std::move(subscriber))));
  }

 private:
  class Subscription : public ObservableOperator<U, D>::Subscription {
   public:
    Subscription(
        Reference<Observable<D>> flowable,
        Reference<Observer<D>> subscriber)
        : ObservableOperator<U, D>::Subscription(
              std::move(flowable),
              std::move(subscriber)) {}

    void onNext(U value) override {
      auto* subscriber =
          ObservableOperator<U, D>::Subscription::subscriber_.get();
      auto* flowable = ObservableOperator<U, D>::Subscription::flowable_.get();
      auto* map = static_cast<MapOperator*>(flowable);
      subscriber->onNext(map->function_(std::move(value)));
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

  void subscribe(Reference<Observer<U>> subscriber) override {
    ObservableOperator<U, U>::upstream_->subscribe(
        // Note: implicit cast to a reference to a subscriber.
        Reference<Subscription>(new Subscription(
            Reference<Observable<U>>(this), std::move(subscriber))));
  }

 private:
  class Subscription : public ObservableOperator<U, U>::Subscription {
   public:
    Subscription(
        Reference<Observable<U>> flowable,
        Reference<Observer<U>> subscriber)
        : ObservableOperator<U, U>::Subscription(
              std::move(flowable),
              std::move(subscriber)) {}

    void onNext(U value) override {
      auto* subscriber =
          ObservableOperator<U, U>::Subscription::subscriber_.get();
      auto* flowable = ObservableOperator<U, U>::Subscription::flowable_.get();
      auto* filter = static_cast<FilterOperator*>(flowable);
      if (filter->function_(value)) {
        subscriber->onNext(std::move(value));
      }
    }
  };

  F function_;
};

template <typename T>
class TakeOperator : public ObservableOperator<T, T> {
 public:
  TakeOperator(Reference<Observable<T>> upstream, int64_t limit)
      : ObservableOperator<T, T>(std::move(upstream)), limit_(limit) {}

  void subscribe(Reference<Observer<T>> subscriber) override {
    ObservableOperator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Observable<T>>(this), limit_, std::move(subscriber))));
  }

 private:
  class Subscription : public ObservableOperator<T, T>::Subscription {
   public:
    Subscription(
        Reference<Observable<T>> flowable,
        int64_t limit,
        Reference<Observer<T>> subscriber)
        : ObservableOperator<T, T>::Subscription(
              std::move(flowable),
              std::move(subscriber)),
          limit_(limit) {}

    void onNext(T value) override {
      if (limit_-- > 0) {
        if (pending_ > 0)
          --pending_;
        ObservableOperator<T, T>::Subscription::subscriber_->onNext(
            std::move(value));
        if (limit_ == 0) {
          ObservableOperator<T, T>::Subscription::cancel();
          ObservableOperator<T, T>::Subscription::onComplete();
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
class SubscribeOnOperator : public ObservableOperator<T, T> {
 public:
  SubscribeOnOperator(Reference<Observable<T>> upstream, Scheduler& scheduler)
      : ObservableOperator<T, T>(std::move(upstream)),
        worker_(scheduler.createWorker()) {}

  void subscribe(Reference<Observer<T>> subscriber) override {
    ObservableOperator<T, T>::upstream_->subscribe(
        Reference<Subscription>(new Subscription(
            Reference<Observable<T>>(this),
            std::move(worker_),
            std::move(subscriber))));
  }

 private:
  class Subscription : public ObservableOperator<T, T>::Subscription {
   public:
    Subscription(
        Reference<Observable<T>> flowable,
        std::unique_ptr<Worker> worker,
        Reference<Observer<T>> subscriber)
        : ObservableOperator<T, T>::Subscription(
              std::move(flowable),
              std::move(subscriber)),
          worker_(std::move(worker)) {}

    void cancel() override {
      worker_->schedule([this] { this->callSuperCancel(); });
    }

    void onNext(T value) override {
      auto* subscriber =
          ObservableOperator<T, T>::Subscription::subscriber_.get();
      subscriber->onNext(std::move(value));
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

  void subscribe(Reference<Observer<T>> subscriber) override {
    function_(std::move(subscriber));
  }

 private:
  OnSubscribe function_;
};

} // observable
} // yarpl
