// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamResponder.h"

namespace reactivesocket {

void StreamResponder::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  if (StreamAutomatonBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  publisherSubscribe(subscription);
}

void StreamResponder::onNextImpl(Payload response) noexcept {
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

void StreamResponder::onCompleteImpl() noexcept {
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

void StreamResponder::onErrorImpl(folly::exception_wrapper ex) noexcept {
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

void StreamResponder::pauseStream(RequestHandler& requestHandler) {
  pausePublisherStream(requestHandler);
}

void StreamResponder::resumeStream(RequestHandler& requestHandler) {
  resumePublisherStream(requestHandler);
}

void StreamResponder::endStream(StreamCompletionSignal signal) {
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

void StreamResponder::onNextFrame(Frame_CANCEL&& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::CLOSED:
      break;
  }
}

void StreamResponder::onNextFrame(Frame_REQUEST_N&& frame) {
  PublisherBase::processRequestN(frame.requestN_);
}
}
