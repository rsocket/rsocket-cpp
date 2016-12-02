// Copyright 2004-present Facebook. All Rights Reserved.

#include "ChannelResponder.h"

namespace reactivesocket {

void ChannelResponder::onSubscribeImpl(
    std::shared_ptr<Subscription> subscription) {
  Base::onSubscribe(subscription);
}

void ChannelResponder::onNextImpl(Payload response) {
  switch (state_) {
    case State::RESPONDING:
      Base::onNext(std::move(response));
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::onCompleteImpl() {
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

void ChannelResponder::onErrorImpl(folly::exception_wrapper ex) {
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

void ChannelResponder::requestImpl(size_t n) {
  switch (state_) {
    case State::RESPONDING:
      Base::request(n);
    case State::CLOSED:
      break;
  }
}

void ChannelResponder::cancelImpl() {
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
  Base::onNextFrame(std::move(frame));
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

std::ostream& ChannelResponder::logPrefix(std::ostream& os) {
  return os << "ChannelResponder(" << &connection_ << ", " << streamId_
            << "): ";
}
}
