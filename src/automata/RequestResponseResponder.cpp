// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/RequestResponseResponder.h"
#include <folly/ExceptionString.h>

namespace reactivesocket {

using namespace yarpl;
using namespace yarpl::flowable;

void RequestResponseResponder::onSubscribe(
    Reference<yarpl::flowable::Subscription> subscription) noexcept {
  if (StreamAutomatonBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(std::move(subscription));
}

void RequestResponseResponder::onNext(Payload response) noexcept {
  debugCheckOnNextOnError();
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      writePayload(std::move(response), true);
      closeStream(StreamCompletionSignal::COMPLETE);
      break;
    }
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onComplete() noexcept {
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      completeStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onError(
    const std::exception_ptr ex) noexcept {
  debugCheckOnNextOnError();
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      applicationError(folly::exceptionStr(ex).toStdString());
    } break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::pauseStream(RequestHandler& requestHandler) {
  pausePublisherStream(requestHandler);
}

void RequestResponseResponder::resumeStream(RequestHandler& requestHandler) {
  resumePublisherStream(requestHandler);
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
  terminatePublisher(signal);
  StreamAutomatonBase::endStream(signal);
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

void RequestResponseResponder::handleRequestN(uint32_t n) {
  PublisherBase::processRequestN(n);
}

} // reactivesocket
