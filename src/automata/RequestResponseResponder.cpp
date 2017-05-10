// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/RequestResponseResponder.h"

namespace reactivesocket {

void RequestResponseResponder::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  if (StreamAutomatonBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(subscription);
}

void RequestResponseResponder::onNextImpl(Payload response) noexcept {
  debugCheckOnNextOnCompleteOnError();
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      debugCheckOnNextOnCompleteOnError();
      writePayload(std::move(response), true);
      closeStream(StreamCompletionSignal::COMPLETE);
      break;
    }
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onCompleteImpl() noexcept {
  debugCheckOnNextOnCompleteOnError();
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      completeStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onErrorImpl(
    folly::exception_wrapper ex) noexcept {
  debugCheckOnNextOnCompleteOnError();
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      applicationError(ex.what().toStdString());
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
