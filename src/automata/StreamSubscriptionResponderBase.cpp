// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

void StreamSubscriptionResponderBase::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  if (StreamAutomatonBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(subscription);
}

void StreamSubscriptionResponderBase::onNextImpl(Payload response) noexcept {
  debugCheckOnNextOnCompleteOnError();
  switch (state_) {
    case State::RESPONDING: {
      debugCheckOnNextOnCompleteOnError();
      writePayload(std::move(response), false);
      break;
    }
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionResponderBase::onCompleteImpl() noexcept {
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

void StreamSubscriptionResponderBase::onErrorImpl(
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

void StreamSubscriptionResponderBase::pauseStream(
    RequestHandler& requestHandler) {
  pausePublisherStream(requestHandler);
}

void StreamSubscriptionResponderBase::resumeStream(
    RequestHandler& requestHandler) {
  resumePublisherStream(requestHandler);
}

void StreamSubscriptionResponderBase::endStream(StreamCompletionSignal signal) {
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

void StreamSubscriptionResponderBase::onNextFrame(Frame_CANCEL&& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionResponderBase::onNextFrame(Frame_REQUEST_N&& frame) {
  PublisherBase::processRequestN(frame.requestN_);
}
}
