// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Frame.h"
#include "src/SubscriberBase.h"
#include "src/automata/StreamAutomatonBase.h"
#include "src/mixins/PublisherMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a RequestResponse
/// responder
class RequestResponseResponder : public StreamAutomatonBase,
                                 public PublisherMixin,
                                 public SubscriberBase {
 public:
  struct Parameters : StreamAutomatonBase::Parameters {
    Parameters(
        const typename StreamAutomatonBase::Parameters& baseParams,
        folly::Executor& _executor)
        : StreamAutomatonBase::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit RequestResponseResponder(const Parameters& params)
      : ExecutorBase(params.executor),
        StreamAutomatonBase(params),
        PublisherMixin(1) {}

 private:
  /// @{
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;
  /// @}

  using StreamAutomatonBase::onNextFrame;
  void onNextFrame(Frame_CANCEL&&) override;
  void onNextFrame(Frame_REQUEST_N&&) override;

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
