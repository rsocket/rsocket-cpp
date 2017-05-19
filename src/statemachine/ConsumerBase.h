// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <cstddef>
#include <iostream>
#include "RSocketStateMachine.h"
#include "StreamStateMachineBase.h"
#include "src/Payload.h"
#include "src/internal/AllowanceSemaphore.h"
#include "src/internal/Common.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

enum class StreamCompletionSignal;

/// A class that represents a flow-control-aware consumer of data.
class ConsumerBase : public StreamStateMachineBase,
                     public yarpl::flowable::Subscription {
  using Base = StreamStateMachineBase;

 public:
  using Base::Base;

  /// Adds implicit allowance.
  ///
  /// This portion of allowance will not be synced to the remote end, but will
  /// count towards the limit of allowance the remote PublisherBase may use.
  void addImplicitAllowance(size_t n) {
    allowance_.release(n);
  }

  /// @{
  void subscribe(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber);

  void generateRequest(size_t n);
  /// @}

 protected:
  /// @{
  void endStream(StreamCompletionSignal signal) override;

//  void pauseStream(RequestHandler& requestHandler) override;
//  void resumeStream(RequestHandler& requestHandler) override;

  void processPayload(Payload&&, bool onNext);

  void onError(folly::exception_wrapper ex);
  /// @}

 private:
  // we don't want derived classes to call these methods.
  // derived classes should be calling implementation methods, not the top level
  // methods which are for the application code.
  // avoiding potential bugs..
  using Subscription::request;
  using Subscription::cancel;

  void sendRequests();

  void handleFlowControlError();

  /// A Subscriber that will consume payloads.
  /// This is responsible for delivering a terminal signal to the
  /// Subscriber once the stream ends.
  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> consumingSubscriber_;

  /// A total, net allowance (requested less delivered) by this consumer.
  AllowanceSemaphore allowance_;
  /// An allowance that have yet to be synced to the other end by sending
  /// REQUEST_N frames.
  AllowanceSemaphore pendingAllowance_;
};
}
