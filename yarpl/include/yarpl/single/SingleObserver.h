// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Refcounted.h"
#include "yarpl/single/SingleSubscription.h"

#include <folly/ExceptionWrapper.h>

#include <glog/logging.h>

namespace yarpl {
namespace single {

template <typename T>
class SingleObserver : public virtual Refcounted {
 public:
  // Note: If any of the following methods is overridden in a subclass, the new
  // methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<SingleSubscription> subscription) {
    DCHECK(subscription);

    if (subscription_) {
      subscription->cancel();
      return;
    }

    subscription_ = std::move(subscription);
  }

  virtual void onSuccess(T) {
    DCHECK(subscription_) << "Calling onSuccess() without a subscription";
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(folly::exception_wrapper) {
    DCHECK(subscription_) << "Calling onError() without a subscription";
    subscription_.reset();
  }

 protected:
  SingleSubscription* subscription() {
    return subscription_.operator->();
  }

 private:
  Reference<SingleSubscription> subscription_;
};

/// Specialization of SingleObserver<void>.
template <>
class SingleObserver<void> : public virtual Refcounted {
 public:
  // Note: If any of the following methods is overridden in a subclass, the new
  // methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<SingleSubscription> subscription) {
    DCHECK(subscription);

    if (subscription_) {
      subscription->cancel();
      return;
    }

    subscription_ = std::move(subscription);
  }

  virtual void onSuccess() {
    DCHECK(subscription_) << "Calling onSuccess() without a subscription";
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(folly::exception_wrapper) {
    DCHECK(subscription_) << "Calling onError() without a subscription";
    subscription_.reset();
  }

 protected:
  SingleSubscription* subscription() {
    return subscription_.operator->();
  }

 private:
  Reference<SingleSubscription> subscription_;
};
}
}
