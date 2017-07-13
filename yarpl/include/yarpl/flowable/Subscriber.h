// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdexcept>

#include "yarpl/Refcounted.h"
#include "yarpl/flowable/Subscription.h"

namespace yarpl {
namespace flowable {

template <typename T>
class Subscriber : public virtual Refcounted {
 public:
  // Note: if any of the following methods is overridden in a subclass,
  // the new methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<Subscription> subscription) {
    subscription_ = subscription;
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onComplete() {
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(std::exception_ptr) {
    subscription_.reset();
  }

  virtual void onNext(T) = 0;

 protected:
  Subscription* subscription() {
    return subscription_.operator->();
  }

 private:
  // "Our" reference to the subscription, to ensure that it is retained
  // while calls to its methods are in-flight.
  Reference<Subscription> subscription_{nullptr};
};

} // flowable
} // yarpl
