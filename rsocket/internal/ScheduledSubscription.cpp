// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/ScheduledSubscription.h"

namespace rsocket {

ScheduledSubscription::ScheduledSubscription(
    std::shared_ptr<yarpl::flowable::Subscription> inner,
    folly::EventBase& eventBase)
    : inner_{std::move(inner)}, eventBase_{eventBase} {}

void ScheduledSubscription::request(int64_t n) {
  if (eventBase_.isInEventBaseThread()) {
    inner_->request(n);
  } else {
    eventBase_.runInEventBaseThread([inner = inner_, n] { inner->request(n); });
  }
}

void ScheduledSubscription::cancel() {
  if (eventBase_.isInEventBaseThread()) {
    auto inner = std::move(inner_);
    inner->cancel();
  } else {
    eventBase_.runInEventBaseThread(
        [inner = std::move(inner_)] { inner->cancel(); });
  }
}

} // namespace rsocket
