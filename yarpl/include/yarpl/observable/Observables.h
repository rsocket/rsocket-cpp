// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <limits>

#include "yarpl/observable/Observable.h"
#include "yarpl/observable/Subscriptions.h"

namespace yarpl {
namespace observable {

class Observables {
 public:
  static Reference<Observable<int64_t>> range(int64_t start, int64_t end) {
    auto lambda = [start, end](Reference<Observer<int64_t>> observer) {
      observer->onSubscribe(Subscriptions::empty());
      for (int64_t i = start; i < end; ++i) {
        observer->onNext(i);
      }
      observer->onComplete();
    };

    return Observable<int64_t>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Observable<T>> just(const T& value) {
    auto lambda = [value](Reference<Observer<T>> observer) {
      observer->onSubscribe(Subscriptions::empty());
      observer->onNext(value);
      observer->onComplete();
    };

    return Observable<T>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Observable<T>> justN(std::initializer_list<T> list) {
    std::vector<T> vec(list);

    auto lambda = [v = std::move(vec)](Reference<Observer<T>> observer) {
      observer->onSubscribe(Subscriptions::empty());
      for (auto const& elem : v) {
        observer->onNext(elem);
      }
      observer->onComplete();
    };

    return Observable<T>::create(std::move(lambda));
  }

  // this will generate an observable which can be subscribed to only once
  template <typename T>
  static Reference<Observable<T>> justOnce(T value) {
    auto lambda = [ value = std::move(value), used = false ](
        Reference<Observer<T>> observer) mutable {
      observer->onSubscribe(Subscriptions::empty());

      if (used) {
        observer->onError(
            std::runtime_error("justOnce value was already used"));
        return;
      }

      used = true;
      observer->onNext(std::move(value));
      observer->onComplete();
    };

    return Observable<T>::create(std::move(lambda));
  }

  template <
      typename T,
      typename OnSubscribe,
      typename = typename std::enable_if<
          std::is_callable<OnSubscribe(Reference<Observer<T>>), void>::value>::
          type>
  static Reference<Observable<T>> create(OnSubscribe function) {
    return make_ref<FromPublisherOperator<T, OnSubscribe>>(std::move(function));
  }

  template <typename T>
  static Reference<Observable<T>> empty() {
    auto lambda = [](Reference<Observer<T>> observer) {
      observer->onSubscribe(Subscriptions::empty());
      observer->onComplete();
    };
    return Observable<T>::create(std::move(lambda));
  }

  template <typename T>
  static Reference<Observable<T>> error(folly::exception_wrapper ex) {
    auto lambda = [ex = std::move(ex)](Reference<Observer<T>> observer) {
      observer->onSubscribe(Subscriptions::empty());
      observer->onError(std::move(ex));
    };
    return Observable<T>::create(std::move(lambda));
  }

  template <typename T, typename ExceptionType>
  static Reference<Observable<T>> error(const ExceptionType& ex) {
    auto lambda = [ex = std::move(ex)](Reference<Observer<T>> observer) {
      observer->onSubscribe(Subscriptions::empty());
      observer->onError(std::move(ex));
    };
    return Observable<T>::create(std::move(lambda));
  }

 private:
  Observables() = delete;
};

} // observable
} // yarpl
