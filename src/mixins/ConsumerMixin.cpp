// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/mixins/ConsumerMixin.h"

#include <glog/logging.h>
#include <algorithm>
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"

namespace reactivesocket {

void ConsumerMixin::processPayload(Payload&& payload) {
  if (payload) {
    // Frames carry application-level payloads are taken into account when
    // figuring out flow control allowance.
    if (allowance_.tryAcquire()) {
      sendRequests();
      consumingSubscriber_.onNext(std::move(payload));
    } else {
      handleFlowControlError();
      return;
    }
  }
}

void ConsumerMixin::onError(folly::exception_wrapper ex) {
  consumingSubscriber_.onError(std::move(ex));
}

void ConsumerMixin::sendRequests() {
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

void ConsumerMixin::handleFlowControlError() {
  consumingSubscriber_.onError(std::runtime_error("surplus response"));
  Base::connection_->outputFrameOrEnqueue(
      Frame_CANCEL(Base::streamId_).serializeOut());
}
}
