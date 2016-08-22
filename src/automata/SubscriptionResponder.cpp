// Copyright 2004-present Facebook. All Rights Reserved.

#include "SubscriptionResponder.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

void SubscriptionResponderBase::onNextFrame(Frame_REQUEST_SUB&& frame) {
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
  Base::onNextFrame(frame);
  if (end) {
    connection_->endStream(streamId_, StreamCompletionSignal::GRACEFUL);
  }
}

std::ostream& SubscriptionResponderBase::logPrefix(std::ostream& os) {
  return os << "SubscriptionResponder(" << &connection_ << ", " << streamId_
            << "): ";
}
}
