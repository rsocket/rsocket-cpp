// Copyright 2004-present Facebook. All Rights Reserved.

#include "StreamResponder.h"

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <glog/logging.h>

#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

void StreamResponderBase::onNextFrame(Frame_REQUEST_STREAM&& frame) {
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

std::ostream& StreamResponderBase::logPrefix(std::ostream& os) {
  return os << "StreamResponder(" << &connection_ << ", " << streamId_ << "): ";
}
}
