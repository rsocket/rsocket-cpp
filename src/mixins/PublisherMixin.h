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

#include "src/mixins/ConsumerMixin.h"

namespace reactivesocket {

enum class StreamCompletionSignal;

/// A mixin that represents a flow-control-aware producer of data.
class PublisherMixin {
 public:
  explicit PublisherMixin(uint32_t initialRequestN)
      : initialRequestN_(initialRequestN) {}

  /// @{
  void onSubscribe(std::shared_ptr<Subscription> subscription) {
    debugCheckOnSubscribe();
    producingSubscription_.reset(std::move(subscription));
    if (initialRequestN_) {
      producingSubscription_.request(initialRequestN_.drain());
    }
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
  }

  /// @{

  void endStream(StreamCompletionSignal signal) {
    producingSubscription_.cancel();
  }

  void pauseStream(RequestHandler& requestHandler) {
    if (producingSubscription_) {
      requestHandler.onSubscriptionPaused(producingSubscription_);
    }
  }

  void resumeStream(RequestHandler& requestHandler) {
    if (producingSubscription_) {
      requestHandler.onSubscriptionResumed(producingSubscription_);
    }
  }

  void onNextFrame(Frame_REQUEST_N&& frame) {
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
