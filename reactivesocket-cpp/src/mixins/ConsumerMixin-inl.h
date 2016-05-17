// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once
#include "ConsumerMixin.h"

#include <algorithm>

#include <glog/logging.h>

#include "reactivesocket-cpp/src/ConnectionAutomaton.h"
#include "reactivesocket-cpp/src/Frame.h"
#include "reactivesocket-cpp/src/Payload.h"
#include "reactivesocket-cpp/src/ReactiveStreamsCompat.h"

namespace lithium {
namespace reactivesocket {

template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::onNextFrame(Frame& frame) {
  if (frame.data_) {
    // Frames carry application-level payloads are taken into account when
    // figuring out flow control allowance.
    if (allowance_.tryAcquire()) {
      sendRequests();
      consumingSubscriber_.onNext(std::move(frame.data_));
    } else {
      handleFlowControlError();
      return;
    }
  }
  // After the applocation-level payload is delivered we inspect the frame's
  // metadata, as it could carry information important for other mixins.
  Base::onNextFrame(frame);
}

template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::sendRequests() {
  // TODO(stupaq): batch if remote end has some spare allowance
  // TODO(stupaq): limit how much is synced to the other end
  size_t toSync = Frame_REQUEST_N::kMaxRequestN;
  toSync = pendingAllowance_.drainWithLimit(toSync);
  if (toSync > 0) {
    Frame_REQUEST_N frame(Base::streamId_, static_cast<uint32_t>(toSync));
    Base::connection_->onNextFrame(frame);
  }
}

template <typename Frame, typename Base>
void ConsumerMixin<Frame, Base>::handleFlowControlError() {
  // TODO(stupaq): communicate flow control error and close the stream
  CHECK(false);
}
}
}
