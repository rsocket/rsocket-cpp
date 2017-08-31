// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Refcounted.h"
#include "yarpl/observable/Subscriptions.h"

#include <folly/ExceptionWrapper.h>

#include <glog/logging.h>

namespace yarpl {
namespace observable {

template <typename T>
class Observer : public virtual Refcounted {
 public:
  // Note: If any of the following methods is overridden in a subclass, the new
  // methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<Subscription> subscription) {
    DCHECK(subscription);

    if (subscription_) {
      DLOG(ERROR) << "attempt to double subscribe";
      subscription->cancel();
      return;
    }

    subscription_ = std::move(subscription);
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onComplete() {
    DCHECK(subscription_) << "Calling onComplete() without a subscription";
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(folly::exception_wrapper) {
    DCHECK(subscription_) << "Calling onError() without a subscription";
    subscription_.reset();
  }

  virtual void onNext(T) = 0;

  bool isUnsubscribed() const {
    CHECK(subscription_);
    return subscription_->isCancelled();
  }

  // Ability to add more subscription objects which will be notified when the
  // subscription has been cancelled.
  // Note that calling cancel on the tied subscription is not going to cancel
  // this subscriber
  void addSubscription(Reference<Subscription> subscription) {
    if(!subscription_) {
      subscription->cancel();
      return;
    }
    subscription_->tieSubscription(std::move(subscription));
  }

  template<typename OnCancel>
  void addSubscription(OnCancel onCancel) {
    addSubscription(Subscriptions::create(std::move(onCancel)));
  }

  bool isUnsubscribedOrTerminated() const {
    return !subscription_ || subscription_->isCancelled();
  }

 protected:
  Subscription* subscription() {
    return subscription_.operator->();
  }

  void unsubscribe() {
    CHECK(subscription_);
    subscription_->cancel();
  }

 private:
  Reference<Subscription> subscription_;
};
}
}
