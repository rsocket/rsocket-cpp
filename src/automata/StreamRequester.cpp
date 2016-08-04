// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamRequester.h"

#include <algorithm>
#include <iostream>

namespace reactivesocket {

void StreamRequesterBase::onNext(Payload request) {
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
    case State::CLOSED:
      break;
  }
}

std::ostream& StreamRequesterBase::logPrefix(std::ostream& os) {
  return os << "StreamRequester(" << &connection_ << ", " << streamId_ << "): ";
}
}
