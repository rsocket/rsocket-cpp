// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamSubscriptionRequesterBase.h"
#include <folly/MoveWrapper.h>

namespace reactivesocket {

void StreamSubscriptionRequesterBase::onNext(Payload request) {
  auto movedPayload = folly::makeMoveWrapper(std::move(request));
  auto thisPtr = EnableSharedFromThisBase<
      StreamSubscriptionRequesterBase>::shared_from_this();
  runInExecutor([thisPtr, movedPayload]() mutable {
    thisPtr->onNextImpl(movedPayload.move());
  });
}

void StreamSubscriptionRequesterBase::onNextImpl(Payload request) {
  switch (state_) {
    case State::NEW: {
      state_ = State::REQUESTED;
      // FIXME: find a root cause of this assymetry; the problem here is that
      // the Base::request might be delivered after the whole thing is shut
      // down, if one uses InlineConnection.
      size_t initialN = initialResponseAllowance_.drainWithLimit(
          Frame_REQUEST_N::kMaxRequestN);
      size_t remainingN = initialResponseAllowance_.drain();
      // Send as much as possible with the initial request.
      CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);
      auto flags = initialN > 0 ? FrameFlags_REQN_PRESENT : FrameFlags_EMPTY;

      // We must inform ConsumerMixin about an implicit allowance we have
      // requested from the remote end.
      addImplicitAllowance(initialN);
      sendRequestFrame(flags, initialN, std::move(request));
      // Pump the remaining allowance into the ConsumerMixin _after_ sending the
      // initial request.
      if (remainingN) {
        Base::request(remainingN);
      }
    } break;
    case State::REQUESTED:
      break;
    case State::CLOSED:
      break;
  }
}

void StreamSubscriptionRequesterBase::requestImpl(size_t n) {
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

void StreamSubscriptionRequesterBase::cancelImpl() {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      connection_->outputFrameOrEnqueue(Frame_CANCEL(streamId_).serializeOut());
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

void StreamSubscriptionRequesterBase::onNextFrame(Frame_RESPONSE&& frame) {
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
  Base::onNextFrame(std::move(frame));
  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

void StreamSubscriptionRequesterBase::onNextFrame(Frame_ERROR&& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      Base::onError(std::runtime_error(frame.payload_.moveDataToString()));
      connection_->endStream(streamId_, StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}
}
