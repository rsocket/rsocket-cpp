// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/utils/type_traits.h"

#include "yarpl/single/Single.h"
#include "yarpl/single/SingleSubscriptions.h"

namespace yarpl {
namespace single {

class Singles {
 public:
  template <typename T>
  static Reference<Single<T>> just(const T& value) {
    auto lambda = [value](Reference<SingleObserver<T>> observer) {
      observer->onSubscribe(SingleSubscriptions::empty());
      observer->onSuccess(value);
    };

    return Single<T>::create(std::move(lambda));
  }

  template <
      typename T,
      typename OnSubscribe,
      typename = typename std::enable_if<std::is_callable<
          OnSubscribe(Reference<SingleObserver<T>>),
          void>::value>::type>
  static Reference<Single<T>> create(OnSubscribe function) {
    return make_ref<FromPublisherOperator<T, OnSubscribe>>(std::move(function));
  }

  template <typename T>
  static Reference<Single<T>> error(folly::exception_wrapper ex) {
    auto lambda = [e = std::move(ex)](Reference<SingleObserver<T>> observer) {
      observer->onSubscribe(SingleSubscriptions::empty());
      observer->onError(e);
    };
    return Single<T>::create(std::move(lambda));
  }

  template <typename T, typename ExceptionType>
  static Reference<Single<T>> error(const ExceptionType& ex) {
    auto lambda = [ex](Reference<SingleObserver<T>> observer) {
      observer->onSubscribe(SingleSubscriptions::empty());
      observer->onError(ex);
    };
    return Single<T>::create(std::move(lambda));
  }

  template <typename T, typename TGenerator>
  static Reference<Single<T>> fromGenerator(TGenerator generator) {
    auto lambda = [generator = std::move(generator)](
        Reference<SingleObserver<T>> observer) mutable {
      observer->onSubscribe(SingleSubscriptions::empty());
      observer->onSuccess(generator());
    };
    return Single<T>::create(std::move(lambda));
  }

 private:
  Singles() = delete;
};

} // observable
} // yarpl
