// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include "yarpl/utils/type_traits.h"

#include "yarpl/Disposable.h"
#include "yarpl/Observable_Observer.h"
#include "yarpl/Observable_Subscription.h"

#include "yarpl/observable/sources/Observable_RangeSubscription.h"

#include "yarpl/observable/operators/Observable_Map.h"
#include "yarpl/observable/operators/Observable_Take.h"

namespace yarpl {
namespace observable {

template <typename T>
class ObservableEmitter;

/**
 * Observable type for async push streams.
 *
 * Use Flowable if the data source can be pulled from.
 *
 * Convert from Observable with a BackpressureStrategy if a Flowable
 * is needed for sending over async boundaries, such as a network.
 *
 * For example:
 *
 * someObservable->toFlowable(BackpressureStategy::Drop)
 *
 * @tparam T
 */
template <typename T>
class Observable : public std::enable_shared_from_this<Observable<T>> {
  friend class Observables;

 public:
  Observable(Observable&&) = delete;
  Observable(const Observable&) = delete;
  Observable& operator=(Observable&&) = delete;
  Observable& operator=(const Observable&) = delete;
  virtual ~Observable() = default;

  virtual void subscribe(std::unique_ptr<Observer<T>>) = 0;

  /**
    * Create an Observable<T> with a function that is executed when
    * Observable.subscribe is called.
    *
    * The first call to `observer` should be `observer->onSubscribe(s)`
    * with a Subscription that allows cancellation.
    *
    * Use Subscriptions::create for common implementations.
    *
    * Consider using 'createWithEmitter` if checking for cancellation
    * in a loop is the intended behavior.
    *
    *@tparam F
    *@param function
    *@return
    */
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(std::unique_ptr<Observer<T>>), void>::value>::type>
  static std::shared_ptr<Observable<T>> create(F&& function) {
    // a private method for now until a proper name is figured out
    return std::make_shared<Derived<F>>(std::forward<F>(function));
  }

  /**
   * Create an Observable<T> with a function that is executed when
   * Observable.subscribe is called.
   *
   * The function receives an ObservableEmitter for emitting events
   * and that allows checking for cancellation.
   *
   *@tparam F
   *@param function
   *@return
   */
  template <
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<ObservableEmitter<T>>),
          void>::value>::type>
  static std::shared_ptr<Observable<T>> createWithEmitter(F&& function) {
    return create([f = std::move(function)](auto observer) {
      f(std::make_unique<ObservableEmitter<T>>(std::move(observer)));
    });
  }

  /**
   * Lift an operator into O<T> and return O<R>
   * @tparam R
   * @tparam F
   * @param onSubscribeLift
   * @return
   */
  template <
      typename R,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<yarpl::observable::Observer<R>>),
          std::unique_ptr<yarpl::observable::Observer<T>>>::value>::type>
  std::shared_ptr<Observable<R>> lift(F&& onSubscribeLift) {
    return Observable<R>::create([
      shared_this = this->shared_from_this(),
      onSub = std::move(onSubscribeLift)
    ](auto sOfR) mutable {
      shared_this->subscribe(std::move(onSub(std::move(sOfR))));
    });
  }

  /**
   * Map O<T> -> O<R>
   *
   * @tparam F
   * @param function
   * @return
   */
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  std::shared_ptr<Observable<typename std::result_of<F(T)>::type>> map(
      F&& function) {
    return lift<typename std::result_of<F(T)>::type>(
        yarpl::observable::operators::
            ObservableMapOperator<T, typename std::result_of<F(T)>::type, F>(
                std::forward<F>(function)));
  }

  /**
   * Take n items from O<T> then cancel.
   * @param toTake
   * @return
   */
  std::shared_ptr<Observable<T>> take(int64_t toTake) {
    return lift<T>(
        yarpl::observable::operators::ObservableTakeOperator<T>(toTake));
  }

 protected:
  Observable() = default;

 private:
  template <typename Function>
  class Derived : public Observable {
   public:
    explicit Derived(Function&& function)
        : function_(std::forward<Function>(function)) {}

    void subscribe(std::unique_ptr<Observer<T>> subscriber) override {
      (function_)(std::move(subscriber));
    }

   private:
    Function function_;
  };
};

class Observables {
 public:
  Observables() = default;
  Observables(Observables&&) = delete;
  Observables(const Observables&) = delete;
  Observables& operator=(Observables&&) = delete;
  Observables& operator=(const Observables&) = delete;

  /**
   * Create a F<T> with an onSubscribe function that takes S<T>
   *
   * @tparam F
   * @param function
   * @return
   */
  template <
      typename T,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<yarpl::observable::Observer<T>>),
          void>::value>::type>
  static std::shared_ptr<Observable<T>> unsafeCreate(F&& function) {
    return Observable<T>::create(std::forward<F>(function));
  }

  static std::shared_ptr<Observable<long>> range(long start, long count) {
    return Observable<long>::create(
        yarpl::observable::sources::RangeSubscription(start, count));
  }
};

/**
 * ObservableEmitter used with Observable::createWithEmitter
 *
 * @tparam T
 */
template <typename T>
class ObservableEmitter {
 public:
  explicit ObservableEmitter(std::unique_ptr<yarpl::observable::Observer<T>> observer)
      : observer_(std::move(observer)),
        subscription_(std::make_unique<AtomicBoolSubscription>()) {
    // send Subscription down to Observer before we start emitting
    observer_->onSubscribe(subscription_.get());
  }
  void onNext(const T& t) {
    observer_->onNext(t);
  }
  void onNext(T&& t) {
    observer_->onNext(t);
  }
  void onComplete() {
    observer_->onComplete();
  }
  void onError(const std::exception_ptr error) {
    observer_->onError(error);
  }
  bool isCancelled() {
    return subscription_->isCancelled();
  }

 private:
  std::unique_ptr<yarpl::observable::Observer<T>> observer_;
  std::unique_ptr<Subscription> subscription_;
};
}
}
