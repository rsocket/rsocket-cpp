// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include <type_traits>

#include <glog/logging.h>

#include <reactive-streams/utilities/SmartPointers.h>
#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

enum class StreamCompletionSignal;

/// A mixin that represents a flow-control-aware producer of data.
template <typename ProducedFrame, typename Base>
class PublisherMixin : public Base {
 public:
  using Base::Base;

  /// @{
  void onSubscribe(std::shared_ptr<Subscription> subscription) {
    if (Base::isTerminated()) {
      subscription->cancel();
      return;
    }

    debugCheckOnSubscribe();
    producingSubscription_.reset(std::move(subscription));
    if (initialRequestN_) {
      producingSubscription_.request(initialRequestN_);
    }
  }

  void onNext(Payload payload, FrameFlags flags = FrameFlags_EMPTY) {
    debugCheckOnNextOnCompleteOnError();
    ProducedFrame frame(Base::streamId_, flags, std::move(payload));
    Base::connection_->outputFrameOrEnqueue(frame.serializeOut());
  }
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "PublisherMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

  void onCleanResume() {
    Base::requestHandler_->handleCleanResume(producingSubscription_);
    Base::onCleanResume();
  }
  void onDirtyResume() {
    Base::requestHandler_->handleDirtyResume(producingSubscription_);
    Base::onDirtyResume();
  }

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
  void endStream(StreamCompletionSignal signal) {
    producingSubscription_.cancel();
    Base::endStream(signal);
  }

  /// Intercept frames that carry allowance.
  template <typename Frame>
  typename std::enable_if<Frame::Trait_CarriesAllowance>::type onNextFrame(
      Frame&& frame) {
    // if producingSubscription_ == nullptr that means the instance is
    // new and onSubscribe hasn't been called yet or it is terminated
    if (size_t n = frame.requestN_) {
      if (producingSubscription_) {
        producingSubscription_.request(n);
      } else {
        initialRequestN_ += n;
      }
    }
    Base::onNextFrame(std::move(frame));
  }

  void onNextFrame(Frame_REQUEST_RESPONSE&& frame) {
    // if producingSubscription_ == nullptr that means the instance is
    // new and onSubscribe hasn't been called yet or it is terminated
    if (producingSubscription_) {
      producingSubscription_.request(1);
    } else {
      initialRequestN_ = 1;
    }
    Base::onNextFrame(std::move(frame));
  }

  /// Remaining frames just pass through.
  template <typename Frame>
  typename std::enable_if<!Frame::Trait_CarriesAllowance>::type onNextFrame(
      Frame&& frame) {
    Base::onNextFrame(std::move(frame));
  }
  /// @}

 private:
  /// A Subscription that constrols production of payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscription once the stream ends.
  reactivestreams::SubscriptionPtr<Subscription> producingSubscription_;
  size_t initialRequestN_{0};
};
}
