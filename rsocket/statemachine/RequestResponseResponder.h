// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/Payload.h"
#include "rsocket/statemachine/StreamStateMachineBase.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscription.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a RequestResponse
/// responder
class RequestResponseResponder : public StreamStateMachineBase,
                                 public yarpl::single::SingleObserver<Payload> {
 public:
  RequestResponseResponder(
      std::shared_ptr<StreamsWriter> writer,
      StreamId streamId)
      : StreamStateMachineBase(std::move(writer), streamId) {}

  void onSubscribe(std::shared_ptr<yarpl::single::SingleSubscription>) override;
  void onSuccess(Payload) override;
  void onError(folly::exception_wrapper) override;

  void handleCancel() override;

  void endStream(StreamCompletionSignal) override;

 private:
  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  };

  std::shared_ptr<yarpl::single::SingleSubscription> producingSubscription_;
  State state_{State::RESPONDING};
};

} // namespace rsocket
