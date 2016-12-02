// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/Frame.h"
#include "src/SubscriberBase.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/PublisherMixin.h"
#include "src/mixins/StreamIfMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a RequestResponse
/// responder
class RequestResponseResponder
    : public StreamIfMixin<PublisherMixin<Frame_RESPONSE, MixinTerminator>>,
      public SubscriberBase {
  using Base = StreamIfMixin<PublisherMixin<Frame_RESPONSE, MixinTerminator>>;

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

  std::ostream& logPrefix(std::ostream& os);

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;
  void onNextFrame(Frame_CANCEL&&) override;
  /// @}

 private:
  /// @{
  void onSubscribeImpl(std::shared_ptr<Subscription>) override;
  void onNextImpl(Payload) override;
  void onCompleteImpl() override;
  void onErrorImpl(folly::exception_wrapper) override;
  /// @}

  void endStream(StreamCompletionSignal) override;

  /// State of the Subscription responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};

} // reactivesocket
