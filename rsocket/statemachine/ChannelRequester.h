// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "rsocket/Payload.h"
#include "rsocket/statemachine/ConsumerBase.h"
#include "rsocket/statemachine/PublisherBase.h"
#include "yarpl/flowable/Subscriber.h"

namespace folly {
class exception_wrapper;
}

namespace rsocket {

/// Implementation of stream stateMachine that represents a Channel requester.
class ChannelRequester : public ConsumerBase,
                         public PublisherBase,
                         public yarpl::flowable::Subscriber<Payload> {
 public:
  explicit ChannelRequester(const ConsumerBase::Parameters& params);

 private:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override;
  void onNext(Payload) noexcept override;
  void onComplete() noexcept override;
  void onError(std::exception_ptr) noexcept override;

  // implementation from ConsumerBase::SubscriptionBase
  void request(int64_t) noexcept override;
  void cancel() noexcept override;

  void handlePayload(Payload&& payload, bool complete, bool flagsNext) override;
  void handleRequestN(uint32_t n) override;
  void handleError(folly::exception_wrapper errorPayload) override;
  void handleCancel() override;

  void endStream(StreamCompletionSignal) override;
  void tryCompleteChannel();

  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerBase.
  AllowanceSemaphore initialResponseAllowance_;
  bool requested_{false};
};

} // reactivesocket
