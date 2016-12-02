// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <reactive-streams/utilities/SmartPointers.h>
#include <iosfwd>

#include "src/Frame.h"
#include "src/SubscriptionBase.h"
#include "src/mixins/MixinTerminator.h"
#include "src/mixins/StreamIfMixin.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a RequestResponse
/// requester
class RequestResponseRequester
    : public StreamIfMixin<MixinTerminator>,
      public SubscriptionBase,
      public EnableSharedFromThisBase<RequestResponseRequester> {
  using Base = StreamIfMixin<MixinTerminator>;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit RequestResponseRequester(const Parameters& params)
      : ExecutorBase(params.executor, false), Base(params) {}

  /// Degenerate form of the Subscriber interface -- only one request payload
  /// will be sent to the server.
  // TODO(lehecka): rename to avoid confusion
  void onNext(Payload);

  /// @{
  bool subscribe(std::shared_ptr<Subscriber<Payload>> subscriber);
  /// @}

  /// Not all frames are intercepted, some just pass through.
  using Base::onNextFrame;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;

  std::ostream& logPrefix(std::ostream& os);

 private:
  void requestImpl(size_t) override;
  void cancelImpl() override;

  void onNextImpl(Payload);

  void endStream(StreamCompletionSignal signal) override;

  /// State of the Subscription requester.
  enum class State : uint8_t {
    NEW,
    REQUESTED,
    CLOSED,
  } state_{State::NEW};

  // Whether the Subscriber made the request(1) call and thus is
  // ready to accept the payload.
  bool waitingForPayload_{false};
  Payload payload_;

  /// A Subscriber that will consume payloads.
  /// This mixin is responsible for delivering a terminal signal to the
  /// Subscriber once the stream ends.
  reactivestreams::SubscriberPtr<Subscriber<Payload>> consumingSubscriber_;
};
}
