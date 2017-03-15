// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <yarpl/Flowable.h>
#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace operators {

using reactivestreams_yarpl::Subscriber;

template <typename T, typename R, typename F>
class TransformSubscriber : public Subscriber<T> {
 public:
  TransformSubscriber(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<R>> s,
      F* f)
      : downstream_(std::move(s)), f_(f) {}

  void onSubscribe(reactivestreams_yarpl::Subscription* upstreamSubscription) {
    // just pass it through since map does nothing to the subscription
    downstream_->onSubscribe(upstreamSubscription);
  }

  void onNext(const T& t) {
    downstream_->onNext((*f_)(t));
  }

  void onNext(T&& t) {
    downstream_->onNext((*f_)(std::move(t)));
  }

  void onComplete() {
    downstream_->onComplete();
  }

  void onError(const std::exception_ptr error) {
    downstream_->onError(error);
  }

 private:
  std::unique_ptr<reactivestreams_yarpl::Subscriber<R>> downstream_;
  F* f_;
};

template <typename T, typename R, typename F>
class FlowableMapOperator {
 public:
  FlowableMapOperator(F&& f) : transform_(std::move(f)) {}
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> operator()(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<R>> s) {
    return std::make_unique<TransformSubscriber<T, R, F>>(
        std::move(s), &transform_);
  }

 private:
  F transform_;
};

} // operators
} // yarpl
