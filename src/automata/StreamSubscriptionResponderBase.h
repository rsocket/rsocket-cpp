// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/AbstractStreamAutomaton.h"
#include "src/Frame.h"
#include "src/SubscriberBase.h"
#include "src/automata/StreamAutomatonBase.h"
#include "src/mixins/PublisherMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Stream/Subscription
/// responder.
class StreamSubscriptionResponderBase
    : public PublisherMixin<Frame_RESPONSE, StreamAutomatonBase>,
      public SubscriberBase {
  using Base = PublisherMixin<Frame_RESPONSE, StreamAutomatonBase>;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  // initialization of the ExecutorBase will be ignored for any of the
  // derived classes
  explicit StreamSubscriptionResponderBase(
      uint32_t initialRequestN,
      const Parameters& params)
      : ExecutorBase(params.executor), Base(initialRequestN, params, nullptr) {}

 protected:
  using Base::onNextFrame;
  void onNextFrame(Frame_CANCEL&&) override;

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
}
