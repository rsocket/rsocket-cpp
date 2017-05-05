// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Flowable.h"
#include "yarpl/flowable/utils/SubscriptionHelper.h"

namespace yarpl {
namespace observable {
template <typename T>
class Observable;
}
}

namespace yarpl {
namespace flowable {
namespace sources {

template <typename T>
class FlowableFromObservableSubscription
    : public yarpl::flowable::Subscription,
      public yarpl::observable::Observer<T> {

 public:
  FlowableFromObservableSubscription(
      Reference<yarpl::observable::Observable<T>> observable,
      Reference<yarpl::flowable::Subscriber<T>> s)
      : observable_(std::move(observable)), subscriber_(std::move(s)) {}

  FlowableFromObservableSubscription(FlowableFromObservableSubscription&&) =
      delete;

  FlowableFromObservableSubscription(
      const FlowableFromObservableSubscription&) = delete;
  FlowableFromObservableSubscription& operator=(
      FlowableFromObservableSubscription&&) = delete;
  FlowableFromObservableSubscription& operator=(
      const FlowableFromObservableSubscription&) = delete;

  void request(int64_t n) override {
    if (n <= 0) {
      return;
    }
    int64_t r = internal::SubscriptionHelper::addCredits(&requested_, n);
    if (r <= 0) {
      return;
    }

    if (!started) {
      bool expected = false;
      if (started.compare_exchange_strong(expected, true)) {
        observable_->subscribe(Reference<yarpl::observable::Observer<T>>(this));
      }
    }
  }

  void cancel() override {
    if (yarpl::flowable::internal::SubscriptionHelper::addCancel(&requested_)) {
      // if this is the first time calling cancel, send the cancel
      observableSubscription_->cancel();
      release();
    }
  }

  // Observer override
  void onSubscribe(
      Reference<yarpl::observable::Subscription> subscription) override {
    observableSubscription_ = subscription;
  }

  // Observer override
  void onNext(const T& t) override {
    if (requested_ > 0) {
      subscriber_->onNext(t);
      yarpl::flowable::internal::SubscriptionHelper::consumeCredits(
          &requested_, 1);
    }
    // drop anything else received while we don't have credits
  }

  // Observer override
  void onComplete() override {
    subscriber_->onComplete();
    release();
  }

  // Observer override
  void onError(const std::exception_ptr error) override {
    subscriber_->onError(error);
    release();
  }

 private:
  Reference<yarpl::observable::Observable<T>> observable_;
  Reference<yarpl::flowable::Subscriber<T>> subscriber_;
  std::atomic_bool started{false};
  std::atomic<int64_t> requested_{0};
  Reference<yarpl::observable::Subscription> observableSubscription_;
};
}
}
}
