// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/MoveWrapper.h>
#include "src/EnableSharedFromThis.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ExecutorMixin.h"

namespace reactivesocket {

//
// SubscriberBase is designed to be a general purpose base class for
// implementors of Subscriber<T> interface. It provides 2 key features:
// 1. ExecutorBase: You can specify the Executor you want to use to execute the
//    on{Subscribe,Next,Complete,Error} methods. Eq. by providing EventBase
//    as an executor, you can call the methods from any thread and the
//    marshaling to the right EventBase thread will happen automatically.
// 2. once the user cancels the subscriber via the subscription::cancel, it is
//    guaranteed that no more callbacks will be called. It is safe for the
// client to do cleanup and releasing of resources. User doesn't have to call
//    subscription::cancel in on{Complete,Error} methods.
//
template <typename T>
class SubscriberBaseT : public Subscriber<T>,
                        public EnableSharedFromThisBase<SubscriberBaseT<T>>,
                        public virtual ExecutorBase {
  virtual void onSubscribeImpl(std::shared_ptr<Subscription> subscription) = 0;
  virtual void onNextImpl(T payload) = 0;
  virtual void onCompleteImpl() = 0;
  virtual void onErrorImpl(folly::exception_wrapper ex) = 0;

  // used to be able to cancel subscription immediately, making sure we dont
  // deliver any other signals after that
  // also to break the reference cycle involving storing subscription pointer
  // for the users of the SubscriberBase
  class SubscriptionShim : public Subscription {
   public:
    explicit SubscriptionShim(
        std::shared_ptr<SubscriberBaseT<T>> parentSubscriber)
        : parentSubscriber_(std::move(parentSubscriber)) {}

    void request(size_t n) override final {
      if (auto parent = parentSubscriber_.lock()) {
        parent->runInExecutor([parent, n]() {
          if (!parent->cancelled_) {
            parent->originalSubscription_->request(n);
          }
        });
      }
    }

    void cancel() override final {
      if (auto parent = parentSubscriber_.lock()) {
        if (!parent->cancelled_.exchange(true)) {
          parent->runInExecutor([parent]() {
            parent->originalSubscription_->cancel();
            parent->originalSubscription_ = nullptr;
          });
        }
      }
    }

   private:
    // we don't want to create cycle with parent subscriber. If we do, we would
    // have to make sure the class deriving from SubscriberBase would have to
    // nullify subscription pointer after calling cancel. That is too strong of
    // a requirement for the users.
    std::weak_ptr<SubscriberBaseT<T>> parentSubscriber_;
  };

  friend class SubscriptionShim;

 public:
  // in c++11 we have to declare this explicitly, instead of
  // using ExecutorBase::ExecutorBase because of atomic cancelled :(
  // maybe its gcc issue
  explicit SubscriberBaseT(
      folly::Executor& executor = defaultExecutor(),
      bool startExecutor = true)
      : ExecutorBase(executor, startExecutor), cancelled_(false) {}

  void onSubscribe(std::shared_ptr<Subscription> subscription) override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, subscription]() {
      VLOG(1) << (ExecutorBase*)thisPtr.get() << " onSubscribe";
      CHECK(!thisPtr->originalSubscription_);
      thisPtr->originalSubscription_ = std::move(subscription);
      // if the subscription got cancelled in the meantime, we will not try to
      // subscribe. Instead we will let the instance die when released.
      if (!thisPtr->cancelled_) {
        thisPtr->onSubscribeImpl(
            std::make_shared<SubscriptionShim>(thisPtr->shared_from_this()));
      }
    });
  }

  void onNext(T payload) override final {
    auto movedPayload = folly::makeMoveWrapper(std::move(payload));
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, movedPayload]() mutable {
      VLOG(1) << (ExecutorBase*)thisPtr.get() << " onNext";
      if (!thisPtr->cancelled_) {
        thisPtr->onNextImpl(movedPayload.move());
      }
    });
  }

  void onComplete() override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr]() {
      VLOG(1) << (ExecutorBase*)thisPtr.get() << " onComplete";
      if (!thisPtr->cancelled_.exchange(true)) {
        thisPtr->onCompleteImpl();

        DCHECK(thisPtr->originalSubscription_);
        thisPtr->originalSubscription_->cancel();
        thisPtr->originalSubscription_ = nullptr;
      }
    });
  }

  void onError(folly::exception_wrapper ex) override final {
    auto movedEx = folly::makeMoveWrapper(std::move(ex));
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, movedEx]() mutable {
      VLOG(1) << (ExecutorBase*)thisPtr.get() << " onError";
      if (!thisPtr->cancelled_.exchange(true)) {
        thisPtr->onErrorImpl(movedEx.move());

        DCHECK(thisPtr->originalSubscription_);
        thisPtr->originalSubscription_->cancel();
        thisPtr->originalSubscription_ = nullptr;
      }
    });
  }

 private:
  // Once the subscription is cancelled we will no longer deliver any
  // other signals.
  // Scheduling the calls is not atomic operation so it may very well happen
  // that 2 threads race for sending onNext and onComplete. We need to make sure
  // that once the terminating signal is delivered we no longer try to deliver
  // onNext.
  std::atomic<bool> cancelled_;

  std::shared_ptr<Subscription> originalSubscription_;
};

extern template class SubscriberBaseT<Payload>;
using SubscriberBase = SubscriberBaseT<Payload>;

} // reactivesocket
