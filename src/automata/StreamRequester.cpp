// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamRequester.h"

#include <algorithm>
#include <iostream>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

void StreamRequesterBase::onNext(Payload request) {
  switch (state_) {
    case State::NEW: {
      state_ = State::REQUESTED;

      size_t initialN = initialResponseAllowance_.drainWithLimit(
          Frame_REQUEST_N::kMaxRequestN);
      size_t remainingN = initialResponseAllowance_.drain();
      // Send as much as possible with the initial request.
      CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);
      auto flags = initialN > 0 ? FrameFlags_REQN_PRESENT : FrameFlags_EMPTY;
      Frame_REQUEST_STREAM frame(
          streamId_,
          flags,
          static_cast<uint32_t>(initialN),
          FrameMetadata::empty(),
          std::move(request));
      // We must inform ConsumerMixin about an implicit allowance we have
      // requested from the remote end.
      addImplicitAllowance(initialN);
      connection_->onNextFrame(frame);
      // Pump the remaining allowance into the ConsumerMixin _after_ sending the
      // initial request.
      if (remainingN) {
        Base::request(remainingN);
      }
    } break;
    case State::REQUESTED:
      break;
    case State:: CLOSED:
      break;
  }
}

void StreamRequesterBase::request(size_t n) {
  switch (state_) {
    case State::NEW:
      // The initial request has not been sent out yet, hence we must accumulate
      // the unsynchronised allowance, portion of which will be sent out with
      // the initial request frame, and the rest will be dispatched via
      // Base::request (ultimately by sending REQUEST_N frames).
      initialResponseAllowance_.release(n);
      break;
    case State::REQUESTED:
      Base::request(n);
      break;
    case State::CLOSED:
      break;
  }
}

void StreamRequesterBase::cancel() {
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

void StreamRequesterBase::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::NEW:
    case State::REQUESTED:
      // TODO(jprahman)
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  Base::endStream(signal);
}

void StreamRequesterBase::onNextFrame(Frame_RESPONSE& frame) {
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

void StreamRequesterBase::onNextFrame(Frame_ERROR& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::CLOSED:
      break;
  }
}

std::ostream& StreamRequesterBase::logPrefix(std::ostream& os) {
  return os << "StreamRequester(" << &connection_ << ", " << streamId_
            << "): ";
}
}
