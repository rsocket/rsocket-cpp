// Copyright 2004-present Facebook. All Rights Reserved.

#include "ChannelRequester.h"

#include <algorithm>
#include <iostream>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "reactivesocket-cpp/src/ConnectionAutomaton.h"
#include "reactivesocket-cpp/src/Frame.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

void ChannelRequesterBase::onSubscribe(Subscription& subscription) {
  CHECK(State::NEW == state_);
  Base::onSubscribe(subscription);
  // Request the first payload immediately.
  subscription.request(1);
}

void ChannelRequesterBase::onNext(Payload request) {
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
      Frame_REQUEST_CHANNEL frame(
          streamId_,
          flags,
          static_cast<uint32_t>(initialN),
          std::move(request));
      // We must inform ConsumerMixin about an implicit allowance we have
      // requested from the remote end.
      addImplicitAllowance(initialN);
      connection_.onNextFrame(frame);
      // Pump the remaining allowance into the ConsumerMixin _after_ sending the
      // initial request.
      if (remainingN) {
        Base::request(remainingN);
      }
    } break;
    case State::REQUESTED:
      Base::onNext(std::move(request));
      break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequesterBase::onComplete() {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      Frame_REQUEST_CHANNEL frame(streamId_, FrameFlags_COMPLETE, 0, nullptr);
      connection_.onNextFrame(frame);
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequesterBase::onError(folly::exception_wrapper ex) {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      Frame_CANCEL frame(streamId_);
      connection_.onNextFrame(frame);
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequesterBase::request(size_t n) {
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

void ChannelRequesterBase::cancel() {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      Frame_CANCEL frame(streamId_);
      connection_.onNextFrame(frame);
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
    } break;
    case State::CLOSED:
      break;
  }
}

void ChannelRequesterBase::endStream(StreamCompletionSignal signal) {
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

void ChannelRequesterBase::onNextFrame(Frame_RESPONSE& frame) {
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
    connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

void ChannelRequesterBase::onNextFrame(Frame_ERROR& frame) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      connection_.endStream(streamId_, StreamCompletionSignal::GRACEFUL);
      break;
    case State::CLOSED:
      break;
  }
}

std::ostream& ChannelRequesterBase::logPrefix(std::ostream& os) {
  return os << "ChannelRequester(" << &connection_ << ", " << streamId_
            << "): ";
}
}
}
