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
class ChannelResponder : public PublisherMixin<
                             Frame_RESPONSE,
                             ConsumerMixin<Frame_REQUEST_CHANNEL>>,
                         public SubscriberBase {
  using Base =
      PublisherMixin<Frame_RESPONSE, ConsumerMixin<Frame_REQUEST_CHANNEL>>;

 public:
  explicit ChannelResponder(const Base::Parameters& params)
      : ExecutorBase(params.executor), Base(params) {}

  void processInitialFrame(Frame_REQUEST_CHANNEL&&);

  std::ostream& logPrefix(std::ostream& os);

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;

  // implementation from ConsumerMixin::SubscriptionBase
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  using Base::onNextFrame;
  void onNextFrame(Frame_REQUEST_CHANNEL&&) override;
  void onNextFrame(Frame_CANCEL&&) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Channel responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
} // reactivesocket
