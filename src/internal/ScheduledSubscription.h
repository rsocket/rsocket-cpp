// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/flowable/Subscription.h"

namespace folly {
class EventBase;
}

namespace rsocket {

//
// A decorator of the Subscription object which schedules the method calls on the
// provided EventBase
//
class ScheduledSubscription : public yarpl::flowable::Subscription {
 public:
  ScheduledSubscription(
      yarpl::Reference<yarpl::flowable::Subscription> inner,
      folly::EventBase& eventBase);

  void request(int64_t n) noexcept override;

  void cancel() noexcept override;

 private:
  yarpl::Reference<yarpl::flowable::Subscription> inner_;
  folly::EventBase& eventBase_;
};

} // rsocket
