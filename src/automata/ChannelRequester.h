// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Frame.h"
#include "src/Payload.h"
#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/PublisherMixin.h"

namespace folly {
class exception_wrapper;
}

namespace reactivesocket {

/// Implementation of stream automaton that represents a Channel requester.
class ChannelRequester : public ConsumerMixin,
                         public PublisherMixin,
                         public SubscriberBase {
 public:
  explicit ChannelRequester(const ConsumerMixin::Parameters& params)
      : ExecutorBase(params.executor),
        ConsumerMixin(params),
        PublisherMixin(0) {}

 private:
  /// @{
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;
  /// @}

  // implementation from ConsumerMixin::SubscriptionBase
  void requestImpl(size_t) noexcept override;
  void cancelImpl() noexcept override;

  using StreamAutomatonBase::onNextFrame;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;
  void onNextFrame(Frame_REQUEST_N&&) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Channel requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};
  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerMixin.
  AllowanceSemaphore initialResponseAllowance_;
};

} // reactivesocket
