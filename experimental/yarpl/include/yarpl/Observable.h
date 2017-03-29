// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include "yarpl/utils/type_traits.h"

#include "yarpl/Flowable.h"

#include "yarpl/Disposable.h"
#include "yarpl/Observable_Observer.h"
#include "yarpl/Observable_Subscription.h"

#include "yarpl/observable/sources/Observable_RangeSubscription.h"

#include "yarpl/flowable/sources/Flowable_FromObservable.h"
#include "yarpl/observable/operators/Observable_Map.h"
#include "yarpl/observable/operators/Observable_Take.h"

namespace yarpl {
namespace observable {

// forward declarations
template <typename T>
class ObservableEmitter;
template <typename T, typename OF>
class EmitterSubscription;

/**
*Strategy for backpressure when converting from Observable to Flowable.
*/
enum class BackpressureStrategy { DROP };

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
   * The function receives an ObservableEmitter for emitting events
   * and that allows checking for cancellation.
   *
   * The ObservableEmitter will heap allocation on subscribe, and it
   * automatically manages the memory lifecycle.
   *
   * Rule: Do NOT call onNext methods on ObservableEmitter AFTER any of:
   *
   * - calling onComplete
   * - calling onError
   * - receiving a cancellation
   *
   * For example:
   *
   *   Observable<int>::create([](auto oe) {
   *    int i = 1;
   *    while (!oe.isCancelled()) {
   *      oe.onNext(i++);
   *   }
   *  })
   *
   * The onComplete and onError methods do protect themselves
   * so that this can be written:
   *
   *  Observable<int>::create([](auto oe) {
   *    for (int i = 1; i <= 10 && !oe.isCancelled(); ++i) {
   *      oe.onNext(i);
   *    }
   *    oe.onComplete();
   *  })
   *
   * onNext does not protect itself similarly due to the performance overhead
   * of conditional branching on every onNext.
   *
   *
   *@tparam F
   *@param function
   *@return
   */
  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(ObservableEmitter<T>&), void>::value>::type>
  static std::shared_ptr<Observable<T>> create(F&& function) {
    return unsafeCreate([f = std::move(function)](auto observer) mutable {
      auto e = new EmitterSubscription<T, F>(std::move(observer), std::move(f));
      e->start();
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
    return Observable<R>::unsafeCreate([
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

  /**
   * Convert from Observable to Flowable with a given BackpressureStrategy.
   *
   * Currently the only strategy is DROP.
   *
   * @param strategy
   * @return
   */
  std::shared_ptr<yarpl::flowable::Flowable<T>> toFlowable(
      BackpressureStrategy strategy) {
    // we currently ONLY support the DROP strategy
    // so do not use the strategy parameter for anything
    return yarpl::flowable::Flowable<T>::create([o = this->shared_from_this()](
        auto subscriber) mutable {
      auto s =
          new yarpl::flowable::sources::FlowableFromObservableSubscription<T>(
              std::move(o), std::move(subscriber));
      s->start();
    });
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

  /**
   * Private creator of an Observable.
   *
   * Whatever is passed into this function *SHOULD* heap allocate
   * and manage its own lifecycle. It is suggested to use the
   * ObservableSubscription base type which manages this.
   *
   * If it does not heap allocate on subscribe, then it will
   * only work for synchronous cases.
   *
   * The higher-level Observable::create method provides
   * an abstraction and does this automatically.
   *
   * @tparam F
   * @param function
   * @return
   */
  template <
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<yarpl::observable::Observer<T>>),
          void>::value>::type>
  static std::shared_ptr<Observable<T>> unsafeCreate(F&& function) {
    return std::make_shared<Derived<F>>(std::forward<F>(function));
  }
};

class Observables {
 public:
  Observables() = default;
  Observables(Observables&&) = delete;
  Observables(const Observables&) = delete;
  Observables& operator=(Observables&&) = delete;
  Observables& operator=(const Observables&) = delete;

  /**I
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
      typename T,
      typename F,
      typename = typename std::enable_if<std::is_callable<
          F(std::unique_ptr<yarpl::observable::Observer<T>>),
          void>::value>::type>
  static std::shared_ptr<Observable<T>> unsafeCreate(F&& function) {
    return Observable<T>::unsafeCreate(std::forward<F>(function));
  }

  static std::shared_ptr<Observable<long>> range(long start, long count) {
    return Observable<long>::unsafeCreate([start, count](auto o) {
      auto s = new yarpl::observable::sources::RangeSubscription(
          start, count, std::move(o));
      s->start();
    });
  }
};

/**
 * ObservableEmitter used with Observable::create
 *
 * This is purely an external API to present to the Observable::create function
 * provided by users. It abstracts the underlying Observer, Subscription,
 * lifecycle, etc.
 *
 * @tparam T
 */
template <typename T>
class ObservableEmitter {
 public:
  explicit ObservableEmitter(ObservableSubscription<T>* emitter)
      : emitter_(emitter) {}

  void onNext(const T& t) {
    emitter_->onNext(t);
  }

  void onNext(T&& t) {
    emitter_->onNext(t);
  }

  void onComplete() {
    emitter_->onComplete();
  }

  void onError(const std::exception_ptr error) {
    emitter_->onError(error);
  }

  bool isCancelled() {
    return emitter_->isCancelled();
  }

 private:
  ObservableSubscription<T>* emitter_;
};

template <typename T, typename OF>
class EmitterSubscription : public ObservableSubscription<T> {
 public:
  EmitterSubscription(std::unique_ptr<Observer<T>> observer, OF&& function)
      : ObservableSubscription<T>(std::move(observer)),
        emitter_(this),
        function_(std::move(function)) {}

  void start() override {
    function_(emitter_);
  }

 private:
  ObservableEmitter<T> emitter_;
  OF function_;
};
}
}
