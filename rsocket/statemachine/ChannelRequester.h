// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/Payload.h"
#include "rsocket/statemachine/ConsumerBase.h"
#include "rsocket/statemachine/PublisherBase.h"
#include "yarpl/flowable/Subscriber.h"

namespace rsocket {

/// Implementation of stream stateMachine that represents a Channel requester.
class ChannelRequester : public ConsumerBase,
                         public PublisherBase,
                         public yarpl::flowable::Subscriber<Payload> {
 public:
  ChannelRequester(
      Payload request,
      std::shared_ptr<StreamsWriter> writer,
      StreamId streamId)
      : ConsumerBase(std::move(writer), streamId),
        PublisherBase(0 /*initialRequestN*/),
        request_(std::move(request)),
        hasInitialRequest_(true) {}

  ChannelRequester(std::shared_ptr<StreamsWriter> writer, StreamId streamId)
      : ConsumerBase(std::move(writer), streamId),
        PublisherBase(1 /*initialRequestN*/) {}

  void onSubscribe(std::shared_ptr<yarpl::flowable::Subscription>) override;
  void onNext(Payload) override;
  void onComplete() override;
  void onError(folly::exception_wrapper) override;

  void request(int64_t) override;
  void cancel() override;

  void handlePayload(Payload&& payload, bool complete, bool next) override;
  void handleRequestN(uint32_t) override;
  void handleError(folly::exception_wrapper) override;
  void handleCancel() override;

  void endStream(StreamCompletionSignal) override;

 private:
  void initStream(Payload&&);
  void tryCompleteChannel();

  /// An allowance accumulated before the stream is initialised.  Remaining part
  /// of the allowance is forwarded to the ConsumerBase.
  Allowance initialResponseAllowance_;

  Payload request_;
  bool requested_{false};
  bool hasInitialRequest_{false};
};

} // namespace rsocket
