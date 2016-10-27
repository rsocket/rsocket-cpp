// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cstddef>
#include <iostream>

#include <folly/ExceptionWrapper.h>
#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include <reactive-streams/utilities/SmartPointers.h>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

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
  void subscribe(std::shared_ptr<Subscriber<Payload>> subscriber) {
    DCHECK(!consumingSubscriber_);
    consumingSubscriber_.reset(std::move(subscriber));
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

  std::ostream& logPrefix(std::ostream& os) {
    return os << "ConsumerMixin(" << &this->connection_ << ", "
              << this->streamId_ << "): ";
  }

 protected:
  /// @{
  void endStream(StreamCompletionSignal signal) {
    switch (signal) {
      case StreamCompletionSignal::CONNECTION_ERROR:
        consumingSubscriber_.onError(std::runtime_error("connection closed"));
        break;
      default:
        consumingSubscriber_.onComplete();
        break;
    }
    Base::endStream(signal);
  }

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;

  void onNextFrame(Frame&&);

  void onError(folly::exception_wrapper ex);
  /// @}

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

#include "ConsumerMixin-inl.h"
