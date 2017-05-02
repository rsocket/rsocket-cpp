// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/SubscriberBase.h"
#include "src/automata/PublisherBase.h"
#include "src/automata/StreamAutomatonBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Stream responder
class StreamResponder : public StreamAutomatonBase,
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
  explicit StreamResponder(uint32_t initialRequestN, const Parameters& params)
      : ExecutorBase(params.executor),
        StreamAutomatonBase(params),
        PublisherBase(initialRequestN) {}

 protected:
  void handleCancel() override;
  void handleRequestN(uint32_t n) override;

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
} // reactivesocket
