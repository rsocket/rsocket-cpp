// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/internal/ScheduledSingleSubscription.h"

#include <folly/io/async/EventBase.h>

namespace rsocket {

ScheduledSingleSubscription::ScheduledSingleSubscription(
    yarpl::Reference<yarpl::single::SingleSubscription> inner,
    folly::EventBase& eventBase) : inner_(std::move(inner)),
                                   eventBase_(eventBase) {
}

void ScheduledSingleSubscription::cancel() {
  if (eventBase_.isInEventBaseThread()) {
    inner_->cancel();
  } else {
    eventBase_.runInEventBaseThread([inner = inner_]
    {
      inner->cancel();
    });
  }
}

} // rsocket
