// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/Payload.h"
#include "src/internal/AllowanceSemaphore.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

enum class StreamCompletionSignal;

/// A class that represents a flow-control-aware producer of data.
class PublisherBase {
 public:
  explicit PublisherBase(uint32_t initialRequestN);

  void publisherSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription);

  void checkPublisherOnNext();

  void publisherComplete();
  bool publisherClosed() const;

  void processRequestN(uint32_t requestN);

  void terminatePublisher();

 private:
  /// A Subscription that constrols production of payloads.
  /// This is responsible for delivering a terminal signal to the
  /// Subscription once the stream ends.
  yarpl::Reference<yarpl::flowable::Subscription> producingSubscription_;
  AllowanceSemaphore initialRequestN_;

  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  } state_{State::RESPONDING};
};
}
