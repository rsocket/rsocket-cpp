// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/RequestResponseResponder.h"

namespace reactivesocket {

void RequestResponseResponder::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  if (StreamAutomatonBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  PublisherMixin::onSubscribe(subscription);
}

void RequestResponseResponder::onNextImpl(Payload response) noexcept {
  debugCheckOnNextOnCompleteOnError();
  switch (state_) {
    case State::RESPONDING: {
      state_ = State::CLOSED;
      debugCheckOnNextOnCompleteOnError();
      Frame_RESPONSE frame(streamId_, FrameFlags_COMPLETE, std::move(response));
      connection_->outputFrameOrEnqueue(frame.serializeOut());
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
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
      connection_->outputFrameOrEnqueue(
          Frame_RESPONSE::complete(streamId_).serializeOut());
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
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
      auto msg = ex.what().toStdString();
      connection_->outputFrameOrEnqueue(
          Frame_ERROR::applicationError(streamId_, msg).serializeOut());
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
    } break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::pauseStream(RequestHandler& requestHandler) {
  PublisherMixin::pauseStream(requestHandler);
}

void RequestResponseResponder::resumeStream(RequestHandler& requestHandler) {
  PublisherMixin::resumeStream(requestHandler);
}

void RequestResponseResponder::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::RESPONDING:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::GRACEFUL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  PublisherMixin::endStream(signal);
  StreamAutomatonBase::endStream(signal);
}

void RequestResponseResponder::onNextFrame(Frame_CANCEL&& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::CLOSED:
      break;
  }
}

void RequestResponseResponder::onNextFrame(Frame_REQUEST_N&& frame) {
  PublisherMixin::onNextFrame(std::move(frame));
}

} // reactivesocket
