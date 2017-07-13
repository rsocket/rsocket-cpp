// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/RequestResponseResponder.h"

#include <glog/logging.h>

#include "src/Payload.h"
#include "yarpl/utils/ExceptionString.h"

namespace rsocket {

using namespace yarpl;
using namespace yarpl::flowable;

void RequestResponseResponder::onSubscribe(
    Reference<yarpl::single::SingleSubscription> subscription) noexcept {
  if (StreamStateMachineBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  DCHECK(!producingSubscription_);
  producingSubscription_ = std::move(subscription);
}

void RequestResponseResponder::onSuccess(Payload response) noexcept {
  DCHECK(producingSubscription_) << "didnt call onSubscribe";
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      writePayload(std::move(response), true);
      producingSubscription_ = nullptr;
      closeStream(StreamCompletionSignal::COMPLETE);
      break;
    }
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onError(std::exception_ptr ex) noexcept {
  DCHECK(producingSubscription_);
  producingSubscription_ = nullptr;
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      applicationError(yarpl::exceptionStr(ex));
      closeStream(StreamCompletionSignal::APPLICATION_ERROR);
    } break;
    case State::CLOSED:
      break;
  }
}

//void RequestResponseResponder::pauseStream(RequestHandler& requestHandler) {
//  pausePublisherStream(requestHandler);
//}
//
//void RequestResponseResponder::resumeStream(RequestHandler& requestHandler) {
//  resumePublisherStream(requestHandler);
//}

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
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::CLOSED:
      break;
  }
}

} // reactivesocket
