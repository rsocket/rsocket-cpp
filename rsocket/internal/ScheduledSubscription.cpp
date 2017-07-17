// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/ScheduledSubscription.h"

#include <folly/io/async/EventBase.h>

namespace rsocket {

ScheduledSubscription::ScheduledSubscription(
    yarpl::Reference<yarpl::flowable::Subscription> inner,
    folly::EventBase& eventBase) : inner_(std::move(inner)),
                                   eventBase_(eventBase) {
}

void ScheduledSubscription::request(int64_t n) noexcept {
  if (eventBase_.isInEventBaseThread()) {
    inner_->request(n);
  } else {
    eventBase_.runInEventBaseThread([inner = inner_, n]
    {
      inner->request(n);
    });
  }
}

void ScheduledSubscription::cancel() noexcept {
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
