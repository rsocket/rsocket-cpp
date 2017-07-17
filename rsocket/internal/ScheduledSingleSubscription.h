// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/single/SingleSubscription.h"

namespace folly {
class EventBase;
}

namespace rsocket {

//
// A decorator of the SingleSubscription object which schedules the method calls on the
// provided EventBase
//
class ScheduledSingleSubscription : public yarpl::single::SingleSubscription {
 public:
  ScheduledSingleSubscription(
      yarpl::Reference<yarpl::single::SingleSubscription> inner,
      folly::EventBase& eventBase);

  void cancel() override;

 private:
  yarpl::Reference<yarpl::single::SingleSubscription> inner_;
  folly::EventBase& eventBase_;
};

} // rsocket
