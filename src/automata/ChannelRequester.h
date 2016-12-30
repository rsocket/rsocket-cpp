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
class ChannelRequester : public PublisherMixin<
                             Frame_REQUEST_CHANNEL,
                             ConsumerMixin<Frame_RESPONSE>>,
                         public SubscriberBase,
                         public SubscriptionBase {
  using Base =
      PublisherMixin<Frame_REQUEST_CHANNEL, ConsumerMixin<Frame_RESPONSE>>;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit ChannelRequester(const Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  std::ostream& logPrefix(std::ostream& os);

 private:
  /// @{
  void onSubscribeImpl(std::shared_ptr<Subscription>) override;
  void onNextImpl(Payload) override;
  void onCompleteImpl() override;
  void onErrorImpl(folly::exception_wrapper) override;
  /// @}

  /// @{
  void requestImpl(size_t) override;
  void cancelImpl() override;
  /// @}

  using Base::onNextFrame;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;

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
