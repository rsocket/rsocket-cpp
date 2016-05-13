// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <iostream>

#include "reactive-streams-cpp/utilities/AllowanceSemaphore.h"
#include "reactive-streams-cpp/utilities/SmartPointers.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

enum class StreamCompletionSignal;

/// A mixin that represents a flow-control-aware consumer of data.
template <typename Frame, typename Base>
class ConsumerMixin : public Base {
 public:
  using Base::Base;

  /// Adds implicit allowance.
  ///
  /// This portion of allowance will not be synced to the remote end, but will
  /// count towards the limit of allowance the remote PublisherMixin may use.
  void addImplicitAllowance(size_t n) {
    allowance_.release(n);
  }

  /// @{
  void subscribe(Subscriber<Payload>& subscriber) {
    DCHECK(!consumingSubscriber_);
    consumingSubscriber_.reset(&subscriber);
    // FIXME
    // Subscriber::onSubscribe is delivered externally, as it may attempt to
    // synchronously deliver Subscriber::request.
  }

  void request(size_t n) {
    allowance_.release(n);
    pendingAllowance_.release(n);
    sendRequests();
  }
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal signal) {
    // FIXME: switch on signal and verify that callsites propagate stream errors
    consumingSubscriber_.onComplete();
    Base::endStream(signal);
  }

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame&);
  /// @}

  std::ostream& logPrefix(std::ostream& os) {
    return os << "ConsumerMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

 private:
  void sendRequests();

  void handleFlowControlError();

  /// A Subscriber that will consume payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscriber once the stream ends.
  reactivestreams::SubscriberPtr<Subscriber<Payload>> consumingSubscriber_;

  /// A total, net allowance (requested less delivered) by this consumer.
  reactivestreams::AllowanceSemaphore allowance_;
  /// An allowance that have yet to be synced to the other end by sending
  /// REQUEST_N frames.
  reactivestreams::AllowanceSemaphore pendingAllowance_;
};
}
}

#include "ConsumerMixin-inl.h"
