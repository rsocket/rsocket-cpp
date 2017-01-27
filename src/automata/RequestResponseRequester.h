// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>
#include "src/SmartPointers.h"

#include "src/Frame.h"
#include "src/SubscriptionBase.h"
#include "src/automata/StreamAutomatonBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a RequestResponse
/// requester
class RequestResponseRequester : public StreamAutomatonBase,
                                 public SubscriptionBase {
  using Base = StreamAutomatonBase;

 public:
  struct Parameters : Base::Parameters {
    Parameters(
        const typename Base::Parameters& baseParams,
        folly::Executor& _executor)
        : Base::Parameters(baseParams), executor(_executor) {}
    folly::Executor& executor;
  };

  explicit RequestResponseRequester(const Parameters& params, Payload payload)
      : ExecutorBase(params.executor),
        Base(params),
        initialPayload_(std::move(payload)) {}

  void subscribe(std::shared_ptr<Subscriber<Payload>> subscriber);

  std::ostream& logPrefix(std::ostream& os);

 private:
  void requestImpl(size_t) noexcept override;
  void cancelImpl() noexcept override;

  using Base::onNextFrame;
  void onNextFrame(Frame_RESPONSE&&) override;
  void onNextFrame(Frame_ERROR&&) override;
  void endStream(StreamCompletionSignal signal) override;

  void pauseStream(RequestHandler& requestHandler) override;
  void resumeStream(RequestHandler& requestHandler) override;

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

  /// Initial payload which has to be sent with 1st request.
  Payload initialPayload_;
};
}
