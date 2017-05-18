// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <glog/logging.h>
#include <iostream>
#include <type_traits>
#include "RSocketStateMachine.h"
#include "src/Payload.h"
#include "src/internal/AllowanceSemaphore.h"
#include "src/temporary_home/Executor.h"
#include "src/temporary_home/RequestHandler.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

enum class StreamCompletionSignal;

/// A class that represents a flow-control-aware producer of data.
class PublisherBase {
 public:
  explicit PublisherBase(uint32_t initialRequestN)
      : initialRequestN_(initialRequestN) {}

  /// @{
  void publisherSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) {
    debugCheckOnSubscribe();
    producingSubscription_ = std::move(subscription);
    if (initialRequestN_) {
      producingSubscription_->request(initialRequestN_.drain());
    }
  }

  /// @}

  const yarpl::Reference<yarpl::flowable::Subscription>& subscription() const {
    return producingSubscription_;
  }

  void processRequestN(uint32_t requestN) {
    if (!requestN) {
      return;
    }

    // we might not have the subscription set yet as there can be REQUEST_N
    // frames scheduled on the executor before onSubscribe method
    if (producingSubscription_) {
      producingSubscription_->request(requestN);
    } else {
      initialRequestN_.release(requestN);
    }
  }

 protected:
  void debugCheckOnSubscribe() {
    DCHECK(!producingSubscription_);
  }

  void debugCheckOnNextOnError() {
    DCHECK(producingSubscription_);
  }

  /// @{
  void terminatePublisher(StreamCompletionSignal signal) {
    if (auto subscription = std::move(producingSubscription_)) {
      subscription->cancel();
    }
  }

  void pausePublisherStream(RequestHandler& requestHandler) {
    requestHandler.onSubscriptionPaused(producingSubscription_);
  }

  void resumePublisherStream(RequestHandler& requestHandler) {
    requestHandler.onSubscriptionResumed(producingSubscription_);
  }

 private:
  /// A Subscription that constrols production of payloads.
  /// This is responsible for delivering a terminal signal to the
  /// Subscription once the stream ends.
  yarpl::Reference<yarpl::flowable::Subscription> producingSubscription_;
  AllowanceSemaphore initialRequestN_;
};
}
