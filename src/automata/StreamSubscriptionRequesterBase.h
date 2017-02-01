// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/AbstractStreamAutomaton.h"
#include "src/AllowanceSemaphore.h"
#include "src/Frame.h"
#include "src/SubscriptionBase.h"
#include "src/mixins/ConsumerMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription requester.
class StreamSubscriptionRequesterBase : public ConsumerMixin<Frame_RESPONSE> {
  using Base = ConsumerMixin<Frame_RESPONSE>;

 public:
  // initialization of the ExecutorBase will be ignored for any of the
  // derived classes
  explicit StreamSubscriptionRequesterBase(
      const Base::Parameters& params,
      Payload payload)
      : ExecutorBase(params.executor),
        Base(params),
        initialPayload_(std::move(payload)) {}

 private:
  /// Override in subclass to send the correct type of request frame
  virtual void sendRequestFrame(FrameFlags, size_t, Payload&&) = 0;

  // implementation from ConsumerMixin::SubscriptionBase
  void requestImpl(size_t) noexcept override;
  void cancelImpl() noexcept override;

  using Base::onNextFrame;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerMixin.
  AllowanceSemaphore initialResponseAllowance_;

  /// Initial payload which has to be sent with 1st request.
  Payload initialPayload_;
};
}
