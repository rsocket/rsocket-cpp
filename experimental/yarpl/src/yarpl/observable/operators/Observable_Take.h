// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Observable.h"

namespace yarpl {
namespace observable {
namespace operators {

template <typename T>
class TakeObserver : public yarpl::observable::Observer<T> {
 public:
  TakeObserver(TakeObserver&&) = default; // only allow std::move
  TakeObserver(const TakeObserver&) = delete;
  TakeObserver& operator=(TakeObserver&&) = default; // only allow std::move
  TakeObserver& operator=(const TakeObserver&) = delete;

  TakeObserver(
      std::unique_ptr<yarpl::observable::Observer<T>> s,
      int64_t toTake)
      : downstream_(std::move(s)), toTake_(toTake) {}

  void onSubscribe(yarpl::observable::Subscription* upstreamSubscription) {
    upstreamSubscription_ = upstreamSubscription;
    downstream_->onSubscribe(upstreamSubscription);
    // see if we started with 0
    if (toTake_ <= 0) {
      completeAndCancel();
    }
  }

  void onNext(const T& t) {
    if (toTake_ > 0) {
      downstream_->onNext(t);
      if (--toTake_ == 0) {
        completeAndCancel();
      }
    }
  }

  void onComplete() {
    if (toTake_ > 0) {
      // only emit if we haven't already completed
      // which will have happened if we hit 0
      downstream_->onComplete();
    }
  }

  void onError(const std::exception_ptr error) {
    downstream_->onError(error);
  }

 private:
  std::unique_ptr<yarpl::observable::Observer<T>> downstream_;
  int64_t toTake_;
  yarpl::observable::Subscription* upstreamSubscription_;

  void completeAndCancel() {
    downstream_->onComplete();
    upstreamSubscription_->cancel();
  }
};

template <typename T>
class ObservableTakeOperator {
 public:
  explicit ObservableTakeOperator(int64_t toTake) : toTake_(toTake) {}
  std::unique_ptr<yarpl::observable::Observer<T>> operator()(
      std::unique_ptr<yarpl::observable::Observer<T>> s) {
    return std::make_unique<TakeObserver<T>>(std::move(s), toTake_);
  }

 private:
  int64_t toTake_;
};

} // operators
} // observable
} // yarpl
