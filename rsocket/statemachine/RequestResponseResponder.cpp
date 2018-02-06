// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/statemachine/RequestResponseResponder.h"

#include "rsocket/Payload.h"

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

void RequestResponseResponder::onSubscribe(
    std::shared_ptr<yarpl::single::SingleSubscription> subscription) noexcept {
#ifndef NDEBUG
  DCHECK(!gotOnSubscribe_.exchange(true)) << "Already called onSubscribe()";
#endif

  if (StreamStateMachineBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  producingSubscription_ = std::move(subscription);
}

void RequestResponseResponder::onSuccess(Payload response) noexcept {
#ifndef NDEBUG
  DCHECK(gotOnSubscribe_.load()) << "didnt call onSubscribe";
  DCHECK(!gotTerminating_.exchange(true)) << "Already called onSuccess/onError";
#endif
  if (!producingSubscription_) {
    return;
  }

  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      writePayload(std::move(response), true);
      producingSubscription_ = nullptr;
      removeFromWriter();
      break;
    }
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onError(folly::exception_wrapper ex) noexcept {
#ifndef NDEBUG
  DCHECK(gotOnSubscribe_.load()) << "didnt call onSubscribe";
  DCHECK(!gotTerminating_.exchange(true)) << "Already called onSuccess/onError";
#endif

  producingSubscription_ = nullptr;
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      applicationError(ex.get_exception()->what());
      removeFromWriter();
    } break;
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
  StreamStateMachineBase::endStream(signal);
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
}
