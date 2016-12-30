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
class RequestResponseResponder
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

  explicit RequestResponseResponder(const Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  void processInitialFrame(Frame_REQUEST_RESPONSE&&);

  std::ostream& logPrefix(std::ostream& os);

 private:
  /// @{
  void onSubscribeImpl(std::shared_ptr<Subscription>) override;
  void onNextImpl(Payload) override;
  void onCompleteImpl() override;
  void onErrorImpl(folly::exception_wrapper) override;
  /// @}

  using Base::onNextFrame;
  void onNextFrame(Frame_CANCEL&&) override;

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};

} // reactivesocket
