// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/AllowanceSemaphore.h"
#include "src/automata/ConsumerBase.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

enum class StreamCompletionSignal;

/// Implementation of stream automaton that represents a Stream requester
class StreamRequester : public ConsumerBase {
  using Base = ConsumerBase;

 public:
  // initialization of the ExecutorBase will be ignored for any of the
  // derived classes
  explicit StreamRequester(const Base::Parameters& params, Payload payload)
      : ExecutorBase(params.executor),
        Base(params),
        initialPayload_(std::move(payload)) {}

 private:
  // implementation from ConsumerBase::SubscriptionBase
  void requestImpl(size_t) noexcept override;
  void cancelImpl() noexcept override;

  using Base::onNextFrame;
  void onNextFrame(Frame_PAYLOAD&&) override;
  void onNextFrame(Frame_ERROR&&) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerBase.
  AllowanceSemaphore initialResponseAllowance_;

  /// Initial payload which has to be sent with 1st request.
  Payload initialPayload_;
};
} // reactivesocket
