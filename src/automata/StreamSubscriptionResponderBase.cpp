// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/StreamSubscriptionResponderBase.h"

namespace reactivesocket {

void StreamSubscriptionResponderBase::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  if (StreamAutomatonBase::isTerminated()) {
    subscription->cancel();
    return;
  }
  PublisherMixin::onSubscribe(subscription);
}

void StreamSubscriptionResponderBase::onNextImpl(Payload response) noexcept {
  debugCheckOnNextOnCompleteOnError();
  switch (state_) {
    case State::RESPONDING: {
      debugCheckOnNextOnCompleteOnError();
      Frame_RESPONSE frame(streamId_, FrameFlags_EMPTY, std::move(response));
      connection_->outputFrameOrEnqueue(frame.serializeOut());
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
      connection_->outputFrameOrEnqueue(
          Frame_RESPONSE::complete(streamId_).serializeOut());
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
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
      auto msg = ex.what().toStdString();
      connection_->outputFrameOrEnqueue(
          Frame_ERROR::applicationError(streamId_, msg).serializeOut());
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
    } break;
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionResponderBase::pauseStream(
    RequestHandler& requestHandler) {
  PublisherMixin::pauseStream(requestHandler);
}

void StreamSubscriptionResponderBase::resumeStream(
    RequestHandler& requestHandler) {
  PublisherMixin::resumeStream(requestHandler);
}

void StreamSubscriptionResponderBase::endStream(StreamCompletionSignal signal) {
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

void StreamSubscriptionResponderBase::onNextFrame(Frame_CANCEL&& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionResponderBase::onNextFrame(Frame_REQUEST_N&& frame) {
  PublisherMixin::onNextFrame(std::move(frame));
}
}
