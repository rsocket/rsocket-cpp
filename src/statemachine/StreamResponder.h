// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/statemachine/PublisherBase.h"
#include "src/statemachine/StreamStateMachineBase.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a Stream responder
class StreamResponder : public StreamStateMachineBase,
                        public PublisherBase,
                        public yarpl::flowable::Subscriber<Payload> {
 public:
  // initialization of the ExecutorBase will be ignored for any of the
  // derived classes
  explicit StreamResponder(uint32_t initialRequestN, const Parameters& params)
      : StreamStateMachineBase(params), PublisherBase(initialRequestN) {}

 protected:
  void handleCancel() override;
  void handleRequestN(uint32_t n) override;

 private:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr) noexcept override;

//  void pauseStream(RequestHandler&) override;
//  void resumeStream(RequestHandler&) override;
  void endStream(StreamCompletionSignal) override;
};
} // reactivesocket
