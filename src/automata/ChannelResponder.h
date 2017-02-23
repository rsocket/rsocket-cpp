// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Frame.h"
#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/PublisherMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Channel responder.
class ChannelResponder : public ConsumerMixin,
                         public PublisherMixin,
                         public SubscriberBase {
 public:
  explicit ChannelResponder(
      uint32_t initialRequestN,
      const ConsumerMixin::Parameters& params)
      : ExecutorBase(params.executor),
        ConsumerMixin(params),
        PublisherMixin(initialRequestN) {}

  void processInitialFrame(Frame_REQUEST_CHANNEL&&);

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;

  // implementation from ConsumerMixin::SubscriptionBase
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  using StreamAutomatonBase::onNextFrame;
  void onNextFrame(Frame_REQUEST_CHANNEL&&) override;
  void onNextFrame(Frame_CANCEL&&) override;
  void onNextFrame(Frame_REQUEST_N&&) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Channel responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
} // reactivesocket
