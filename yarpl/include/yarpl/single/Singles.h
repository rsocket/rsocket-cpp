// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/utils/type_traits.h"

#include "yarpl/single/Single.h"

namespace yarpl {
namespace single {

class Singles {
 public:
  template <typename T>
  static Reference<Single<T>> just(const T& value) {
    auto lambda = [value](Reference<SingleObserver<T>> observer) {
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
  static Reference<Single<T>> create(OnSubscribe&& function) {
    return Reference<Single<T>>(new FromPublisherOperator<T, OnSubscribe>(
        std::forward<OnSubscribe>(function)));
  }

  template <typename T>
  static Reference<Single<T>> error(std::exception_ptr ex) {
    auto lambda = [ex](Reference<SingleObserver<T>> observer) {
      observer->onError(ex);
    };
    return Single<T>::create(std::move(lambda));
  }

  template <typename T, typename ExceptionType>
  static Reference<Single<T>> error(const ExceptionType& ex) {
    auto lambda = [ex](Reference<SingleObserver<T>> observer) {
      observer->onError(std::make_exception_ptr(ex));
    };
    return Single<T>::create(std::move(lambda));
  }

 private:
  Singles() = delete;
};

} // observable
} // yarpl
