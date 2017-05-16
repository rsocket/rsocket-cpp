#pragma once

#include "../Refcounted.h"
#include "SingleObserver.h"
#include "SingleObservers.h"
#include "SingleSubscription.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {
namespace single {

template <typename T>
class Single : public virtual Refcounted {
 public:
  virtual void subscribe(Reference<SingleObserver<T>>) = 0;

  /**
   * Subscribe overload that accepts lambdas.
   *
   * @param success
   */
  template <
      typename Success,
      typename = typename std::enable_if<
          std::is_callable<Success(T), void>::value>::type>
  void subscribe(Success&& next) {
    subscribe(SingleObservers::create<T>(next));
  }

  /**
   * Subscribe overload that accepts lambdas.
   *
   * @param success
   * @param error
   */
  template <
      typename Success,
      typename Error,
      typename = typename std::enable_if<
          std::is_callable<Success(T), void>::value &&
          std::is_callable<Error(const std::exception_ptr), void>::value>::type>
  void subscribe(Success&& next, Error&& error) {
    subscribe(SingleObservers::create<T>(next, error));
  }

  template <
      typename OnSubscribe,
      typename = typename std::enable_if<std::is_callable<
          OnSubscribe(Reference<SingleObserver<T>>),
          void>::value>::type>
  static auto create(OnSubscribe&& function) {
    return Reference<Single<T>>(new FromPublisherOperator<OnSubscribe>(
        std::forward<OnSubscribe>(function)));
  }

  template <typename Function>
  auto map(Function&& function);

 private:
  template <typename OnSubscribe>
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
};

// specialization of Single<void>
template <typename OnSubscribe>
class SingleVoidFromPublisherOperator;

template <>
class Single<void> : public virtual Refcounted {
 public:
  virtual void subscribe(Reference<SingleObserver<void>>) = 0;

  /**
   * subscribe overload taking lambda for onSuccess that is called upon writing
   * to the network
   * @tparam Success
   * @param s
   */
  template <
      typename Success,
      typename = typename std::enable_if<
          std::is_callable<Success(), void>::value>::type>
  void subscribe(Success&& s) {
    class SuccessSingleObserver : public SingleObserver<void> {
     public:
      SuccessSingleObserver(Success&& s) : success_{std::move(s)} {}

      void onSubscribe(Reference<SingleSubscription> subscription) override {
        SingleObserver<void>::onSubscribe(std::move(subscription));
      }

      virtual void onSuccess() override {
        success_();
        SingleObserver<void>::onSuccess();
      }

      // No further calls to the subscription after this method is invoked.
      virtual void onError(const std::exception_ptr eptr) override {
        SingleObserver<void>::onError(eptr);
      }

     private:
      Success success_;
    };

    subscribe(make_ref<SuccessSingleObserver>(std::forward<Success>(s)));
  };

  template <
      typename OnSubscribe,
      typename = typename std::enable_if<std::is_callable<
          OnSubscribe(Reference<SingleObserver<void>>),
          void>::value>::type>
  static auto create(OnSubscribe&& function) {
    return Reference<Single<void>>(
        new SingleVoidFromPublisherOperator<OnSubscribe>(
            std::forward<OnSubscribe>(function)));
  }
};

template <typename OnSubscribe>
class SingleVoidFromPublisherOperator : public Single<void> {
 public:
  explicit SingleVoidFromPublisherOperator(OnSubscribe&& function)
      : function_(std::move(function)) {}

  void subscribe(Reference<SingleObserver<void>> subscriber) override {
    function_(std::move(subscriber));
  }

 private:
  OnSubscribe function_;
};

} // observable
} // yarpl

#include "SingleOperator.h"

namespace yarpl {
namespace single {
template <typename T>
template <typename Function>
auto Single<T>::map(Function&& function) {
  using D = typename std::result_of<Function(T)>::type;
  return Reference<Single<D>>(new MapOperator<T, D, Function>(
      Reference<Single<T>>(this), std::forward<Function>(function)));
}

} // single
} // yarpl
