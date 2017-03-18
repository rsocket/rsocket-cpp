// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace operators {

template <typename T>
class TakeSubscriber : public reactivestreams_yarpl::Subscriber<T> {
 public:
  TakeSubscriber(TakeSubscriber&&) = default; // only allow std::move
  TakeSubscriber(const TakeSubscriber&) = delete;
  TakeSubscriber& operator=(TakeSubscriber&&) = default; // only allow std::move
  TakeSubscriber& operator=(const TakeSubscriber&) = delete;

  TakeSubscriber(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> s,
      int64_t toTake)
      : downstream_(std::move(s)), toTake_(toTake) {}

  void onSubscribe(reactivestreams_yarpl::Subscription* upstreamSubscription) {
    upstreamSubscription_ = upstreamSubscription;
    // TODO adjust subscription to request up the smaller of downstream and take
    downstream_->onSubscribe(upstreamSubscription);
    // see if we started with 0
    if (toTake_ <= 0) {
      completeAndCancel();
    }
  }

  void onNext(const T& t) {
    downstream_->onNext(t);
    if (--toTake_ == 0) {
      completeAndCancel();
    }
  }

  void onNext(T&& t) {
    downstream_->onNext(t);
    if (--toTake_ == 0) {
      completeAndCancel();
    }
  }

  void onComplete() {
    downstream_->onComplete();
  }

  void onError(const std::exception_ptr error) {
    downstream_->onError(error);
  }

 private:
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> downstream_;
  int64_t toTake_;
  reactivestreams_yarpl::Subscription* upstreamSubscription_;

  void completeAndCancel() {
    downstream_->onComplete();
    upstreamSubscription_->cancel();
  }
};

template <typename T>
class FlowableTakeOperator {
 public:
  explicit FlowableTakeOperator(int64_t toTake) : toTake_(toTake) {}
  std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> operator()(
      std::unique_ptr<reactivestreams_yarpl::Subscriber<T>> s) {
    return std::make_unique<TakeSubscriber<T>>(std::move(s), toTake_);
  }

 private:
  int64_t toTake_;
};

} // operators
} // yarpl
