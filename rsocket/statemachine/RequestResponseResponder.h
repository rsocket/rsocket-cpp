// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/statemachine/StreamStateMachineBase.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscription.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a RequestResponse
/// responder
class RequestResponseResponder : public StreamStateMachineBase,
                                 public yarpl::single::SingleObserver<Payload> {
 public:
  explicit RequestResponseResponder(const Parameters& params)
      : StreamStateMachineBase(params) {}

 private:
  void onSubscribe(yarpl::Reference<yarpl::single::SingleSubscription>
                       subscription) noexcept override;
  void onSuccess(Payload) noexcept override;
  void onError(std::exception_ptr) noexcept override;

  void handleCancel() override;

//  void pauseStream(RequestHandler&) override;
//  void resumeStream(RequestHandler&) override;
  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};

  yarpl::Reference<yarpl::single::SingleSubscription> producingSubscription_;
};

} // reactivesocket
