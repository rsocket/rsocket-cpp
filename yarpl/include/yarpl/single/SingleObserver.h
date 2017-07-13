// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdexcept>

#include "yarpl/Refcounted.h"
#include "yarpl/single/SingleSubscription.h"

namespace yarpl {
namespace single {

template <typename T>
class SingleObserver : public virtual Refcounted {
 public:
  // Note: if any of the following methods is overridden in a subclass,
  // the new methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<SingleSubscription> subscription) {
    subscription_ = subscription;
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onSuccess(T) {
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(std::exception_ptr) {
    subscription_.reset();
  }

 protected:
  SingleSubscription* subscription() {
    return subscription_.operator->();
  }

 private:
  // "Our" reference to the subscription, to ensure that it is retained
  // while calls to its methods are in-flight.
  Reference<SingleSubscription> subscription_{nullptr};
};

// specialization of SingleObserver<void>
template <>
class SingleObserver<void> : public virtual Refcounted {
 public:
  // Note: if any of the following methods is overridden in a subclass,
  // the new methods SHOULD ensure that these are invoked as well.
  virtual void onSubscribe(Reference<SingleSubscription> subscription) {
    subscription_ = subscription;
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onSuccess() {
    subscription_.reset();
  }

  // No further calls to the subscription after this method is invoked.
  virtual void onError(std::exception_ptr) {
    subscription_.reset();
  }

 protected:
  SingleSubscription* subscription() {
    return subscription_.operator->();
  }

 private:
  // "Our" reference to the subscription, to ensure that it is retained
  // while calls to its methods are in-flight.
  Reference<SingleSubscription> subscription_{nullptr};
};

} // single
} // yarpl
