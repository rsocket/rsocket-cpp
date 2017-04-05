// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Frame.h"
#include "src/SubscriberBase.h"
#include "src/automata/StreamAutomatonBase.h"
#include "src/automata/PublisherBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Stream/Subscription
/// responder.
class StreamSubscriptionResponderBase : public StreamAutomatonBase,
                                        public PublisherBase,
                                        public SubscriberBase {
 public:
  struct Parameters : StreamAutomatonBase::Parameters {
    Parameters(
        const typename StreamAutomatonBase::Parameters& baseParams,
        folly::Executor& _executor)
        : StreamAutomatonBase::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  // initialization of the ExecutorBase will be ignored for any of the
  // derived classes
  explicit StreamSubscriptionResponderBase(
      uint32_t initialRequestN,
      const Parameters& params)
      : ExecutorBase(params.executor),
        StreamAutomatonBase(params),
        PublisherBase(initialRequestN) {}

 protected:
  using StreamAutomatonBase::onNextFrame;
  void onNextFrame(Frame_CANCEL&&) override;
  void onNextFrame(Frame_REQUEST_N&&) override;

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;

  void pauseStream(RequestHandler&) override;
  void resumeStream(RequestHandler&) override;
  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
}
