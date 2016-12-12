// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <reactive-streams/utilities/AllowanceSemaphore.h>
#include "src/AbstractStreamAutomaton.h"
#include "src/Frame.h"
#include "src/SubscriptionBase.h"
#include "src/mixins/ConsumerMixin.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/StreamIfMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Subscription requester.
class StreamSubscriptionRequesterBase
    : public StreamIfMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>>,
      public SubscriptionBase,
      public EnableSharedFromThisBase<StreamSubscriptionRequesterBase> {
  using Base = StreamIfMixin<ConsumerMixin<Frame_RESPONSE, MixinTerminator>>;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit StreamSubscriptionRequesterBase(const Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  /// Degenerate form of the Subscriber interface -- only one request payload
  /// will be sent to the server.
  // TODO(lehecka): rename to avoid confusion
  void onNext(Payload);

  using Base::onNextFrame;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;

 private:
  void onNextImpl(Payload);

  /// Override in subclass to send the correct type of request frame
  virtual void sendRequestFrame(FrameFlags, size_t, Payload&&) = 0;

  void requestImpl(size_t) override;
  void cancelImpl() override;
  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  /// An allowance accumulated before the stream is initialised.
  /// Remaining part of the allowance is forwarded to the ConsumerMixin.
  reactivestreams::AllowanceSemaphore initialResponseAllowance_;
};
}
