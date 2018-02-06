// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <boost/noncopyable.hpp>
#include <folly/ExceptionWrapper.h>
#include <glog/logging.h>
#include "yarpl/Refcounted.h"
#include "yarpl/flowable/Subscription.h"

namespace yarpl {
namespace flowable {

template <typename T>
class Subscriber : boost::noncopyable {
 public:
  virtual ~Subscriber() = default;
  virtual void onSubscribe(std::shared_ptr<Subscription>) = 0;
  virtual void onComplete() = 0;
  virtual void onError(folly::exception_wrapper) = 0;
  virtual void onNext(T) = 0;
};

#define KEEP_REF_TO_THIS()              \
  std::shared_ptr<BaseSubscriber> self; \
  if (keep_reference_to_this) {         \
    self = this->ref_from_this(this);   \
  }

// T : Type of Flowable that this Subscriber operates on
//
// keep_reference_to_this : BaseSubscriber will keep a live reference to
// itself on the stack while in a signaling or requesting method, in case
// the derived class causes all other references to itself to be dropped.
//
// Classes that ensure that at least one reference will stay live can
// use `keep_reference_to_this = false` as an optimization to
// prevent an atomic inc/dec pair
template <typename T, bool keep_reference_to_this = true>
class BaseSubscriber : public Subscriber<T>, public yarpl::enable_get_ref {
 public:
  // Note: If any of the following methods is overridden in a subclass, the new
  // methods SHOULD ensure that these are invoked as well.
  void onSubscribe(std::shared_ptr<Subscription> subscription) final override {
    CHECK(subscription);
    CHECK(!yarpl::atomic_load(&subscription_));

#ifndef NDEBUG
    DCHECK(!gotOnSubscribe_.exchange(true))
        << "Already subscribed to BaseSubscriber";
#endif

    yarpl::atomic_store(&subscription_, std::move(subscription));
    KEEP_REF_TO_THIS();
    onSubscribeImpl();
  }

  // No further calls to the subscription after this method is invoked.
  void onComplete() final override {
#ifndef NDEBUG
    DCHECK(gotOnSubscribe_.load()) << "Not subscribed to BaseSubscriber";
    DCHECK(!gotTerminating_.exchange(true))
        << "Already got terminating signal method";
#endif

    std::shared_ptr<Subscription> null;
    if (auto sub = yarpl::atomic_exchange(&subscription_, null)) {
      KEEP_REF_TO_THIS();
      onCompleteImpl();
      onTerminateImpl();
    }
  }

  // No further calls to the subscription after this method is invoked.
  void onError(folly::exception_wrapper e) final override {
#ifndef NDEBUG
    DCHECK(gotOnSubscribe_.load()) << "Not subscribed to BaseSubscriber";
    DCHECK(!gotTerminating_.exchange(true))
        << "Already got terminating signal method";
#endif

    std::shared_ptr<Subscription> null;
    if (auto sub = yarpl::atomic_exchange(&subscription_, null)) {
      KEEP_REF_TO_THIS();
      onErrorImpl(std::move(e));
      onTerminateImpl();
    }
  }

  void onNext(T t) final override {
#ifndef NDEBUG
    DCHECK(gotOnSubscribe_.load()) << "Not subscibed to BaseSubscriber";
    if (gotTerminating_.load()) {
      VLOG(2) << "BaseSubscriber already got terminating signal method";
    }
#endif

    if (auto sub = yarpl::atomic_load(&subscription_)) {
      KEEP_REF_TO_THIS();
      onNextImpl(std::move(t));
    }
  }

  void cancel() {
    std::shared_ptr<Subscription> null;
    if (auto sub = yarpl::atomic_exchange(&subscription_, null)) {
      KEEP_REF_TO_THIS();
      sub->cancel();
      onTerminateImpl();
    }
#ifndef NDEBUG
    else {
      VLOG(2) << "cancel() on BaseSubscriber with no subscription_";
    }
#endif
  }

  void request(int64_t n) {
    if (auto sub = yarpl::atomic_load(&subscription_)) {
      KEEP_REF_TO_THIS();
      sub->request(n);
    }
#ifndef NDEBUG
    else {
      VLOG(2) << "request() on BaseSubscriber with no subscription_";
    }
#endif
  }

 protected:
  virtual void onSubscribeImpl() = 0;
  virtual void onCompleteImpl() = 0;
  virtual void onNextImpl(T) = 0;
  virtual void onErrorImpl(folly::exception_wrapper) = 0;

  virtual void onTerminateImpl() {}

 private:
  // keeps a reference alive to the subscription
  AtomicReference<Subscription> subscription_;

#ifndef NDEBUG
  std::atomic<bool> gotOnSubscribe_{false};
  std::atomic<bool> gotTerminating_{false};
#endif
};

} // namespace flowable
} // namespace yarpl
