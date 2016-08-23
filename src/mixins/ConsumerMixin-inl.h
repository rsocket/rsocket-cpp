// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once
#include "ConsumerMixin.h"

#include <algorithm>

#include <glog/logging.h>

#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {
template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::onError(folly::exception_wrapper ex) {
  consumingSubscriber_.onError(ex);
};

template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::onNextFrame(Frame&& frame) {
  if (frame.payload_.data) {
    // Frames carry application-level payloads are taken into account when
    // figuring out flow control allowance.
    if (allowance_.tryAcquire()) {
      sendRequests();
      consumingSubscriber_.onNext(std::move(frame.payload_));
    } else {
      handleFlowControlError();
      return;
    }
  }
  // After the application-level payload is delivered we inspect the frame's
  // metadata, as it could carry information important for other mixins.
  Base::onNextFrame(std::move(frame));
}

template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::sendRequests() {
  // TODO(stupaq): batch if remote end has some spare allowance
  // TODO(stupaq): limit how much is synced to the other end
  size_t toSync = Frame_REQUEST_N::kMaxRequestN;
  toSync = pendingAllowance_.drainWithLimit(toSync);
  if (toSync > 0) {
    Base::connection_->outputFrameOrEnqueue(
        Frame_REQUEST_N(Base::streamId_, static_cast<uint32_t>(toSync)).serializeOut());
  }
}

template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::handleFlowControlError() {
  // TODO(stupaq): communicate flow control error and close the stream
  CHECK(false);
}
}
