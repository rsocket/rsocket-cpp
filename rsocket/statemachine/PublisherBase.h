// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/internal/Allowance.h"
#include "yarpl/flowable/Subscription.h"

namespace rsocket {

/// A class that represents a flow-control-aware producer of data.
class PublisherBase {
 public:
  explicit PublisherBase(uint32_t initialRequestN);

  void publisherSubscribe(std::shared_ptr<yarpl::flowable::Subscription>);

  void processRequestN(uint32_t);
  void publisherComplete();

  bool publisherClosed() const;
  void terminatePublisher();

 private:
  enum class State : uint8_t {
    RESPONDING,
    CLOSED,
  };

  std::shared_ptr<yarpl::flowable::Subscription> producingSubscription_;
  Allowance initialRequestN_;
  State state_{State::RESPONDING};
};

} // namespace rsocket
