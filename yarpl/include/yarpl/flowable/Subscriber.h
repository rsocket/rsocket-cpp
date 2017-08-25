// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Refcounted.h"
#include "yarpl/flowable/Subscription.h"

#include <folly/ExceptionWrapper.h>

#include <glog/logging.h>

namespace yarpl {
namespace flowable {

template <typename T>
class Subscriber : public virtual Refcounted {
 public:
  // Note: If any of the following methods is overridden in a subclass, the new
  // methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<Subscription> subscription) {
    DCHECK(subscription);

    if (subscription_) {
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

 protected:
  Subscription* subscription() {
    return subscription_.operator->();
  }

 private:
  Reference<Subscription> subscription_;
};
}
}
