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

  explicit StreamSubscriptionResponderBase(const Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

 protected:
  using Base::onNextFrame;
  void onNextFrame(Frame_CANCEL&&) override;

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) override;
  void onNextImpl(Payload) override;
  void onCompleteImpl() override;
  void onErrorImpl(folly::exception_wrapper) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
}
