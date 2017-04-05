// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace operators {

template <typename T, typename R, typename F>
class TransformSubscriber : public reactivestreams_yarpl::Subscriber<T> {
 public:
  TransformSubscriber(TransformSubscriber&&) = default; // only allow std::move
  TransformSubscriber(const TransformSubscriber&) = delete;
  TransformSubscriber& operator=(TransformSubscriber&&) =
      default; // only allow std::move
  TransformSubscriber& operator=(const TransformSubscriber&) = delete;

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
    // TODO catch error and send via onError
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
  explicit FlowableMapOperator(F&& f) : transform_(std::move(f)) {}
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
