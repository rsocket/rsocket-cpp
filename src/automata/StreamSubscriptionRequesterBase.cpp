// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamSubscriptionRequesterBase.h"

namespace reactivesocket {

void StreamSubscriptionRequesterBase::request(size_t n) {
  switch (state_) {
    case State::NEW:
      // The initial request has not been sent out yet, hence we must accumulate
      // the unsynchronised allowance, portion of which will be sent out with
      // the initial request frame, and the rest will be dispatched via
      // Base:request (ultimately by sending REQUEST_N frames).
      initialResponseAllowance_.release(n);
      break;
    case State::REQUESTED:
      Base::request(n);
      break;
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionRequesterBase::cancel() {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      Frame_CANCEL frame(streamId_);
      connection_->onNextFrame(frame);
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionRequesterBase::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::NEW:
    case State::REQUESTED:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::GRACEFUL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  Base::endStream(signal);
}

void StreamSubscriptionRequesterBase::onNextFrame(Frame_RESPONSE& frame) {
  bool end = false;
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      if (frame.header_.flags_ & FrameFlags_COMPLETE) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }
  Base::onNextFrame(frame);
  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

void StreamSubscriptionRequesterBase::onNextFrame(Frame_ERROR& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      Base::onError(
          std::runtime_error(frame.data_->moveToFbString().toStdString()));
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}
}
