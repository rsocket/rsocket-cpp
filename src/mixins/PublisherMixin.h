// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <glog/logging.h>
#include <iostream>
#include <type_traits>
#include "src/AllowanceSemaphore.h"
#include "src/ConnectionAutomaton.h"
#include "src/Executor.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/RequestHandler.h"
#include "src/SmartPointers.h"

namespace reactivesocket {

enum class StreamCompletionSignal;

/// A mixin that represents a flow-control-aware producer of data.
template <typename ProducedFrame, typename Base>
class PublisherMixin : public Base {
 public:
  explicit PublisherMixin(
      uint32_t initialRequestN,
      const typename Base::Parameters& params)
      : ExecutorBase(params.executor),
        Base(params),
        initialRequestN_(initialRequestN) {}

  explicit PublisherMixin(
      uint32_t initialRequestN,
      const typename Base::Parameters& params,
      std::nullptr_t)
      : Base(params), initialRequestN_(initialRequestN) {}

  /// @{
  void onSubscribe(std::shared_ptr<Subscription> subscription) {
    if (Base::isTerminated()) {
      subscription->cancel();
      return;
    }

    debugCheckOnSubscribe();
    producingSubscription_.reset(std::move(subscription));
    if (initialRequestN_) {
      producingSubscription_.request(initialRequestN_.drain());
    }
  }

  void onNext(Payload payload, FrameFlags flags = FrameFlags_EMPTY) {
    debugCheckOnNextOnCompleteOnError();
    ProducedFrame frame(Base::streamId_, flags, std::move(payload));
    Base::connection_->outputFrameOrEnqueue(frame.serializeOut());
  }
  /// @}

  std::shared_ptr<Subscription> subscription() {
    return producingSubscription_;
  }

 protected:
  void debugCheckOnSubscribe() {
    DCHECK(!producingSubscription_);
  }

  void debugCheckOnNextOnCompleteOnError() {
    DCHECK(producingSubscription_);
    // the previous DCHECK should also cover !Base::isTerminated()
    // but we will make sure that invariant is not broken as well
    DCHECK(!Base::isTerminated());
  }

  /// @{
  void endStream(StreamCompletionSignal signal) override {
    producingSubscription_.cancel();
    Base::endStream(signal);
  }

  void pauseStream(RequestHandler& requestHandler) override {
    if (producingSubscription_) {
      requestHandler.onSubscriptionPaused(producingSubscription_);
    }
  }

  void resumeStream(RequestHandler& requestHandler) override {
    if (producingSubscription_) {
      requestHandler.onSubscriptionResumed(producingSubscription_);
    }
  }

  using Base::onNextFrame;
  void onNextFrame(Frame_REQUEST_N&& frame) override {
    processRequestN(frame.requestN_);
  }

  void processRequestN(uint32_t requestN) {
    if (!requestN) {
      return;
    }

    // we might not have the subscription set yet as there can be REQUEST_N
    // frames scheduled on the executor before onSubscribe method
    if (producingSubscription_) {
      producingSubscription_.request(requestN);
    } else {
      initialRequestN_.release(requestN);
    }
  }

 private:
  /// A Subscription that constrols production of payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscription once the stream ends.
  reactivestreams::SubscriptionPtr<Subscription> producingSubscription_;
  AllowanceSemaphore initialRequestN_;
};
}
