// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include <type_traits>

#include <glog/logging.h>

#include "reactive-streams-cpp/utilities/SmartPointers.h"
#include "reactivesocket-cpp/src/ConnectionAutomaton.h"
#include "reactivesocket-cpp/src/Frame.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

enum class StreamCompletionSignal;

/// A mixin that represents a flow-control-aware producer of data.
template <typename ProducedFrame, typename Base>
class PublisherMixin : public Base {
 public:
  using Base::Base;

  /// @{
  void onSubscribe(Subscription& subscription) {
    DCHECK(!producingSubscription_);
    producingSubscription_.reset(&subscription);
  }

  void onNext(Payload payload) {
    ProducedFrame frame(Base::streamId_, FrameFlags_EMPTY, std::move(payload));
    Base::connection_.onNextFrame(frame);
  }
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal signal) {
    // FIXME: switch on signal
    producingSubscription_.cancel();
    Base::endStream(signal);
  }

  /// Intercept frames that carry allowance.
  template <typename Frame>
  typename std::enable_if<Frame::Trait_CarriesAllowance>::type onNextFrame(
      Frame& frame) {
    if (size_t n = frame.requestN_) {
      producingSubscription_.request(n);
    }
    Base::onNextFrame(frame);
  }

  /// Remaining frames just pass through.
  template <typename Frame>
  typename std::enable_if<!Frame::Trait_CarriesAllowance>::type onNextFrame(
      Frame& frame) {
    Base::onNextFrame(frame);
  }
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "PublisherMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

 private:
  /// A Subscription that constrols production of payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscription once the stream ends.
  reactivestreams::SubscriptionPtr<Subscription> producingSubscription_;
};
}
}
