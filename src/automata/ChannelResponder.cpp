// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/ChannelResponder.h"

namespace reactivesocket {

void ChannelResponder::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) noexcept {
  Base::onSubscribe(subscription);
}

void ChannelResponder::onNextImpl(Payload response) noexcept {
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

void ChannelResponder::onCompleteImpl() noexcept {
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

void ChannelResponder::onErrorImpl(folly::exception_wrapper ex) noexcept {
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

void ChannelResponder::requestImpl(size_t n) noexcept {
  switch (state_) {
    case State::RESPONDING:
      Base::generateRequest(n);
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::cancelImpl() noexcept {
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

void ChannelResponder::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::RESPONDING:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::GRACEFUL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  Base::endStream(signal);
}

void ChannelResponder::processInitialFrame(Frame_REQUEST_CHANNEL&& frame) {
  onNextFrame(std::move(frame));
}

void ChannelResponder::onNextFrame(Frame_REQUEST_CHANNEL&& frame) {
  bool end = false;
  switch (state_) {
    case State::RESPONDING:
      if (frame.header_.flags_ & FrameFlags_COMPLETE) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }

  processRequestN(frame.requestN_);
  processPayload(std::move(frame.payload_));

  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

void ChannelResponder::onNextFrame(Frame_CANCEL&& frame) {
  switch (state_) {
    case State::RESPONDING:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::CLOSED:
      break;
  }
}
}
