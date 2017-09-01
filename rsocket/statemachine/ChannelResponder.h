// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "rsocket/statemachine/ConsumerBase.h"
#include "rsocket/statemachine/PublisherBase.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a Channel responder.
class ChannelResponder : public ConsumerBase,
                         public PublisherBase,
                         public yarpl::flowable::Subscriber<Payload> {
 public:
  ChannelResponder(
      std::shared_ptr<StreamsWriter> writer,
      StreamId streamId,
      uint32_t initialRequestN)
      : ConsumerBase(std::move(writer), streamId),
        PublisherBase(initialRequestN) {}

  void processInitialFrame(Frame_REQUEST_CHANNEL&&);

 private:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload) noexcept override;
  void onComplete() noexcept override;
  void onError(folly::exception_wrapper) noexcept override;

  void request(int64_t n) noexcept override;
  void cancel() noexcept override;

  void handlePayload(Payload&& payload, bool complete, bool flagsNext) override;
  void handleRequestN(uint32_t n) override;
  void handleCancel() override;
  void handleError(folly::exception_wrapper ex) override;

  void onNextPayloadFrame(
      uint32_t requestN,
      Payload&& payload,
      bool complete,
      bool next);

  void endStream(StreamCompletionSignal) override;

  void tryCompleteChannel();
};
}
