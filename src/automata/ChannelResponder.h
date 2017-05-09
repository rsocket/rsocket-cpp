// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iosfwd>

#include "src/SubscriberBase.h"
#include "src/SubscriptionBase.h"
#include "src/automata/ConsumerBase.h"
#include "src/automata/PublisherBase.h"

namespace reactivesocket {

/// Implementation of stream automaton that represents a Channel responder.
class ChannelResponder : public ConsumerBase,
                         public PublisherBase,
                         public SubscriberBase {
 public:
  explicit ChannelResponder(
      uint32_t initialRequestN,
      const ConsumerBase::Parameters& params)
      : ExecutorBase(params.executor),
        ConsumerBase(params),
        PublisherBase(initialRequestN) {}

  void processInitialFrame(Frame_REQUEST_CHANNEL&&);

 private:
  void onSubscribeImpl(std::shared_ptr<Subscription>) noexcept override;
  void onNextImpl(Payload) noexcept override;
  void onCompleteImpl() noexcept override;
  void onErrorImpl(folly::exception_wrapper) noexcept override;

  // implementation from ConsumerBase::SubscriptionBase
  void requestImpl(size_t n) noexcept override;
  void cancelImpl() noexcept override;

  void handlePayload(Payload&& payload, bool complete, bool flagsNext) override;
  void handleRequestN(uint32_t n) override;
  void handleCancel() override;

  void onNextPayloadFrame(
      uint32_t requestN,
      Payload&& payload,
      bool complete,
      bool next);

  void endStream(StreamCompletionSignal) override;

  /// State of the Channel responder.
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
} // reactivesocket
