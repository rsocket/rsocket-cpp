// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Observable.h"

namespace yarpl {
namespace observable {
namespace operators {

template <typename T, typename R, typename F>
class TransformSubscriber : public yarpl::observable::Observer<T> {
 public:
  TransformSubscriber(TransformSubscriber&&) = default; // only allow std::move
  TransformSubscriber(const TransformSubscriber&) = delete;

  TransformSubscriber& operator=(TransformSubscriber&&) =
      default; // only allow std::move
  TransformSubscriber& operator=(const TransformSubscriber&) = delete;

  TransformSubscriber(std::unique_ptr<yarpl::observable::Observer<R>> s, F* f)
      : downstream_(std::move(s)), f_(f) {}

  void onSubscribe(yarpl::observable::Subscription* upstreamSubscription) {
    // just pass it through since map does nothing to the subscription
    downstream_->onSubscribe(upstreamSubscription);
  }

  void onNext(const T& t) {
    downstream_->onNext((*f_)(t));
    // TODO catch error and send via onError
  }

  void onComplete() {
    downstream_->onComplete();
  }

  void onError(const std::exception_ptr error) {
    downstream_->onError(error);
  }

 private:
  std::unique_ptr<yarpl::observable::Observer<R>> downstream_;
  F* f_;
};

template <typename T, typename R, typename F>
class ObservableMapOperator {
 public:
  explicit ObservableMapOperator(F&& f) : transform_(std::move(f)) {}

  std::unique_ptr<yarpl::observable::Observer<T>> operator()(
      std::unique_ptr<yarpl::observable::Observer<R>> s) {
    return std::make_unique<TransformSubscriber<T, R, F>>(
        std::move(s), &transform_);
  }

 private:
  F transform_;
};

} // operators
} // observable
} // yarpl
