// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once
#include "src/mixins/ConsumerMixin.h"

#include <glog/logging.h>
#include <algorithm>
#include "src/ConnectionAutomaton.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {
template <typename Frame>
void ConsumerMixin<Frame>::onError(folly::exception_wrapper ex) {
  consumingSubscriber_.onError(std::move(ex));
}

template <typename Frame>
void ConsumerMixin<Frame>::processPayload(Frame&& frame) {
  if (frame.payload_) {
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
}

template <typename Frame>
void ConsumerMixin<Frame>::sendRequests() {
  // TODO(stupaq): batch if remote end has some spare allowance
  // TODO(stupaq): limit how much is synced to the other end
  size_t toSync = Frame_REQUEST_N::kMaxRequestN;
  toSync = pendingAllowance_.drainWithLimit(toSync);
  if (toSync > 0) {
    Base::connection_->outputFrameOrEnqueue(
        Frame_REQUEST_N(Base::streamId_, static_cast<uint32_t>(toSync))
            .serializeOut());
  }
}

template <typename Frame>
void ConsumerMixin<Frame>::handleFlowControlError() {
  consumingSubscriber_.onError(std::runtime_error("surplus response"));
  Base::connection_->outputFrameOrEnqueue(
      Frame_CANCEL(Base::streamId_).serializeOut());
}
}
