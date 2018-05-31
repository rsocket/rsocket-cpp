// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/PublisherBase.h"

#include <glog/logging.h>

namespace rsocket {

PublisherBase::PublisherBase(uint32_t initialRequestN)
    : initialRequestN_(initialRequestN) {}

void PublisherBase::publisherSubscribe(
    std::shared_ptr<yarpl::flowable::Subscription> subscription) {
  if (state_ == State::CLOSED) {
    subscription->cancel();
    return;
  }
  DCHECK(!producingSubscription_);
  producingSubscription_ = std::move(subscription);
  if (initialRequestN_) {
    producingSubscription_->request(initialRequestN_.consumeAll());
  }
}

void PublisherBase::publisherComplete() {
  state_ = State::CLOSED;
  producingSubscription_ = nullptr;
}

bool PublisherBase::publisherClosed() const {
  return state_ == State::CLOSED;
}

void PublisherBase::processRequestN(uint32_t requestN) {
  if (requestN == 0 || state_ == State::CLOSED) {
    return;
  }

  // We might not have the subscription set yet as there can be REQUEST_N frames
  // scheduled on the executor before onSubscribe method.
  if (producingSubscription_) {
    producingSubscription_->request(requestN);
  } else {
    initialRequestN_.add(requestN);
  }
}

void PublisherBase::terminatePublisher() {
  state_ = State::CLOSED;
  if (auto subscription = std::move(producingSubscription_)) {
    subscription->cancel();
  }
}

} // namespace rsocket
