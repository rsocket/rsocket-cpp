// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/statemachine/PublisherBase.h"
#include "rsocket/statemachine/StreamStateMachineBase.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a Stream responder
class StreamResponder : public StreamStateMachineBase,
                        public PublisherBase,
                        public yarpl::flowable::Subscriber<Payload> {
 public:
  StreamResponder(
      std::shared_ptr<StreamsWriter> writer,
      StreamId streamId,
      uint32_t initialRequestN)
      : StreamStateMachineBase(std::move(writer), streamId),
        PublisherBase(initialRequestN) {}

 protected:
  void handleCancel() override;
  void handleRequestN(uint32_t n) override;

 private:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload) noexcept override;
  void onComplete() noexcept override;
  void onError(folly::exception_wrapper) noexcept override;

  void endStream(StreamCompletionSignal) override;
};
}
