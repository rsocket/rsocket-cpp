// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/statemachine/StreamRequester.h"
#include <folly/MoveWrapper.h>

namespace reactivesocket {

void StreamRequester::request(int64_t n) noexcept {
  if (n == 0) {
    return;
  }

  switch (state_) {
    case State::NEW: {
      state_ = State::REQUESTED;
      // FIXME: find a root cause of this asymmetry; the problem here is that
      // the Base::request might be delivered after the whole thing is shut
      // down, if one uses InlineConnection.
      auto initialN =
          n > Frame_REQUEST_N::kMaxRequestN ? Frame_REQUEST_N::kMaxRequestN : n;
      auto remainingN = n > Frame_REQUEST_N::kMaxRequestN
          ? n - Frame_REQUEST_N::kMaxRequestN
          : 0;

      // Send as much as possible with the initial request.
      CHECK_GE(Frame_REQUEST_N::kMaxRequestN, initialN);

      // We must inform ConsumerBase about an implicit allowance we have
      // requested from the remote end.
      addImplicitAllowance(initialN);
      newStream(
          StreamType::STREAM,
          static_cast<uint32_t>(initialN),
          std::move(initialPayload_));

      // Pump the remaining allowance into the ConsumerBase _after_ sending the
      // initial request.
      if (remainingN) {
        Base::generateRequest(remainingN);
      }
    } break;
    case State::REQUESTED:
      Base::generateRequest(n);
      break;
    case State::CLOSED:
      break;
  }
}

void StreamRequester::cancel() noexcept {
  switch (state_) {
    case State::NEW:
      state_ = State::CLOSED;
      closeStream(StreamCompletionSignal::CANCEL);
      break;
    case State::REQUESTED: {
      state_ = State::CLOSED;
      cancelStream();
    } break;
    case State::CLOSED:
      break;
  }
}

void StreamRequester::endStream(StreamCompletionSignal signal) {
  switch (state_) {
    case State::NEW:
    case State::REQUESTED:
      // Spontaneous ::endStream signal means an error.
      DCHECK(StreamCompletionSignal::COMPLETE != signal);
      DCHECK(StreamCompletionSignal::CANCEL != signal);
      state_ = State::CLOSED;
      break;
    case State::CLOSED:
      break;
  }
  Base::endStream(signal);
}

void StreamRequester::handlePayload(Payload&& payload,
                                    bool complete,
                                    bool flagsNext) {
  bool end = false;
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      if (complete) {
        state_ = State::CLOSED;
        end = true;
      }
      break;
    case State::CLOSED:
      break;
  }

  processPayload(std::move(payload), flagsNext);

  if (end) {
    closeStream(StreamCompletionSignal::COMPLETE);
  }
}

void StreamRequester::handleError(folly::exception_wrapper errorPayload) {
  switch (state_) {
    case State::NEW:
      // Cannot receive a frame before sending the initial request.
      CHECK(false);
      break;
    case State::REQUESTED:
      state_ = State::CLOSED;
      Base::onError(errorPayload);
      closeStream(StreamCompletionSignal::ERROR);
      break;
    case State::CLOSED:
      break;
  }
}
}
