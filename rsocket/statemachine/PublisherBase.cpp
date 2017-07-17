// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/PublisherBase.h"

#include <glog/logging.h>

#include "rsocket/statemachine/RSocketStateMachine.h"

namespace rsocket {

PublisherBase::PublisherBase(uint32_t initialRequestN)
      : initialRequestN_(initialRequestN) {}

void PublisherBase::publisherSubscribe(
    yarpl::Reference<yarpl::flowable::Subscription> subscription) {
  if (state_ == State::CLOSED) {
    subscription->cancel();
    return;
  }
  DCHECK(!producingSubscription_);
  producingSubscription_ = std::move(subscription);
  if (initialRequestN_) {
    producingSubscription_->request(initialRequestN_.drain());
  }
}

void PublisherBase::checkPublisherOnNext() {
  DCHECK(producingSubscription_);
  CHECK(state_ == State::RESPONDING);
}

void PublisherBase::publisherComplete() {
  state_ = State::CLOSED;
  producingSubscription_ = nullptr;
}

bool PublisherBase::publisherClosed() const {
  return state_ == State::CLOSED;
}

void PublisherBase::processRequestN(uint32_t requestN) {
  if (!requestN || state_ == State::CLOSED) {
    return;
  }

  // we might not have the subscription set yet as there can be REQUEST_N
  // frames scheduled on the executor before onSubscribe method
  if (producingSubscription_) {
    producingSubscription_->request(requestN);
  } else {
    initialRequestN_.release(requestN);
  }
}

void PublisherBase::terminatePublisher() {
  state_ = State::CLOSED;
  if (auto subscription = std::move(producingSubscription_)) {
    subscription->cancel();
  }
}
}
