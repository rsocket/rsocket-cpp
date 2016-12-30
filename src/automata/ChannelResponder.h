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
                         public SubscriberBase,
                         public SubscriptionBase {
  using Base =
      PublisherMixin<Frame_RESPONSE, ConsumerMixin<Frame_REQUEST_CHANNEL>>;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit ChannelResponder(const Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  void processInitialFrame(Frame_REQUEST_CHANNEL&&);

  std::ostream& logPrefix(std::ostream& os);

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) override;
  void onNextImpl(Payload) override;
  void onCompleteImpl() override;
  void onErrorImpl(folly::exception_wrapper) override;

  void requestImpl(size_t n) override;
  void cancelImpl() override;

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
