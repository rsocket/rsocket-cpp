// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>

#include "yarpl/flowable/Subscription.h"

namespace rsocket {

// A wrapper over Subscription that schedules all of the subscription's methods
// on an EventBase.
class ScheduledSubscription : public yarpl::flowable::Subscription {
 public:
  ScheduledSubscription(
      std::shared_ptr<yarpl::flowable::Subscription>,
      folly::EventBase&);

  void request(int64_t) override;
  void cancel() override;

 private:
  std::shared_ptr<yarpl::flowable::Subscription> inner_;
  folly::EventBase& eventBase_;
};

} // namespace rsocket
