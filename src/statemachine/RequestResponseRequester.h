// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/Payload.h"
#include "src/statemachine/StreamStateMachineBase.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscription.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a RequestResponse
/// requester
class RequestResponseRequester : public StreamStateMachineBase,
                                 public yarpl::single::SingleSubscription {
  using Base = StreamStateMachineBase;

 public:
  explicit RequestResponseRequester(const Parameters& params, Payload payload)
      : Base(params), initialPayload_(std::move(payload)) {}

  void subscribe(
      yarpl::Reference<yarpl::single::SingleObserver<Payload>> subscriber);

 private:
  void cancel() noexcept override;

  void handlePayload(Payload&& payload, bool complete, bool flagsNext) override;
  void handleError(folly::exception_wrapper errorPayload) override;

  void endStream(StreamCompletionSignal signal) override;

//  void pauseStream(RequestHandler& requestHandler) override;
//  void resumeStream(RequestHandler& requestHandler) override;

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  /// The observer that will consume payloads.
  yarpl::Reference<yarpl::single::SingleObserver<Payload>> consumingSubscriber_;

  /// Initial payload which has to be sent with 1st request.
  Payload initialPayload_;
};
}
