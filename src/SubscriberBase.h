// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/MoveWrapper.h>
#include "src/EnableSharedFromThis.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/mixins/ExecutorMixin.h"

namespace reactivesocket {

template <typename T>
class SubscriberBaseT : public Subscriber<T>,
                        public EnableSharedFromThisBase<SubscriberBaseT<T>>,
                        public virtual ExecutorBase {
  virtual void onSubscribeImpl(std::shared_ptr<Subscription> subscription) = 0;
  virtual void onNextImpl(T payload) = 0;
  virtual void onCompleteImpl() = 0;
  virtual void onErrorImpl(folly::exception_wrapper ex) = 0;

 public:
  using ExecutorBase::ExecutorBase;

  void onSubscribe(std::shared_ptr<Subscription> subscription) override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, subscription]() {
      thisPtr->onSubscribeImpl(subscription);
    });
  }

  void onNext(T payload) override final {
    auto movedPayload = folly::makeMoveWrapper(std::move(payload));
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr, movedPayload]() mutable {
      thisPtr->onNextImpl(movedPayload.move());
    });
  }

  void onComplete() override final {
    auto thisPtr = this->shared_from_this();
    runInExecutor([thisPtr]() {
      thisPtr->onCompleteImpl();
    });
  }

  void onError(folly::exception_wrapper ex) override final {
    auto movedEx = folly::makeMoveWrapper(std::move(ex));
    auto thisPtr = this->shared_from_this();
    runInExecutor(
        [thisPtr, movedEx]() mutable { thisPtr->onErrorImpl(movedEx.move()); });
  }
};

extern template class SubscriberBaseT<Payload>;
using SubscriberBase = SubscriberBaseT<Payload>;

} // reactivesocket
