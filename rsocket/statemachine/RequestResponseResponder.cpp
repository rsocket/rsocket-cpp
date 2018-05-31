// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/RequestResponseResponder.h"

namespace rsocket {

void RequestResponseResponder::onSubscribe(
    std::shared_ptr<yarpl::single::SingleSubscription> subscription) {
  if (state_ == State::CLOSED) {
    subscription->cancel();
    return;
  }
  producingSubscription_ = std::move(subscription);
}

void RequestResponseResponder::onSuccess(Payload response) {
  if (!producingSubscription_) {
    return;
  }

  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      writePayload(std::move(response), true /* complete */);
      producingSubscription_ = nullptr;
      removeFromWriter();
      break;
    }
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onError(folly::exception_wrapper ex) {
  producingSubscription_ = nullptr;
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      writeApplicationError(ex.get_exception()->what());
      removeFromWriter();
    } break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::handleCancel() {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      removeFromWriter();
      break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::RESPONDING:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::COMPLETE != signal);
      DCHECK(StreamCompletionSignal::CANCEL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  if (auto subscription = std::move(producingSubscription_)) {
    subscription->cancel();
  }
}

} // namespace rsocket
