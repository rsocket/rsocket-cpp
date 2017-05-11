// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Payload.h"
#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"
#include "src/automata/ConsumerBase.h"
#include "src/automata/PublisherBase.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

/// Implementation of stream automaton that represents a Channel requester.
class ChannelRequester : public ConsumerBase,
                         public PublisherBase,
                         public SubscriberBase {
 public:
  explicit ChannelRequester(const ConsumerBase::Parameters& params)
      : ExecutorBase(params.executor), ConsumerBase(params), PublisherBase(0) {}

 private:
  /// @{
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;
  /// @}

  // implementation from ConsumerBase::SubscriptionBase
  void requestImpl(size_t) noexcept override;
  void cancelImpl() noexcept override;

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
