// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Payload.h"
#include "src/temporary_home/SubscriberBase.h"
#include "src/statemachine/ConsumerBase.h"
#include "src/statemachine/PublisherBase.h"
#include "yarpl/flowable/Subscriber.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

/// Implementation of stream automaton that represents a Channel requester.
class ChannelRequester : public ConsumerBase,
                         public PublisherBase,
                         public yarpl::flowable::Subscriber<Payload> {
 public:
  explicit ChannelRequester(const ConsumerBase::Parameters& params)
      : ConsumerBase(params), PublisherBase(0) {}

 private:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription> subscription) noexcept override;
  void onNext(Payload) noexcept override;
  void onComplete() noexcept override;
  void onError(const std::exception_ptr) noexcept override;

  // implementation from ConsumerBase::SubscriptionBase
  void request(int64_t) noexcept override;
  void cancel() noexcept override;

  void handlePayload(Payload&& payload, bool complete, bool flagsNext) override;
  void handleRequestN(uint32_t n) override;
  void handleError(folly::exception_wrapper errorPayload) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Channel requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};
  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerBase.
  AllowanceSemaphore initialResponseAllowance_;
};

} // reactivesocket
