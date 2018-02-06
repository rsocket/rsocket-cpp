// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/statemachine/StreamStateMachineBase.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/single/SingleObserver.h"
#include "yarpl/single/SingleSubscription.h"

#include <glog/logging.h>

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

 private:
  void onSubscribe(std::shared_ptr<yarpl::single::SingleSubscription>
                       subscription) noexcept override;
  void onSuccess(Payload) noexcept override;
  void onError(folly::exception_wrapper) noexcept override;

  void handleCancel() override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};

  std::shared_ptr<yarpl::single::SingleSubscription> producingSubscription_;
#ifndef NDEBUG
  std::atomic<bool> gotOnSubscribe_{false};
  std::atomic<bool> gotTerminating_{false};
#endif
};
}
