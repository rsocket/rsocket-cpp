// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/SubscriptionResponder.h"
#include "src/Frame.h"

namespace reactivesocket {

void SubscriptionResponder::onCompleteImpl() noexcept {
  LOG(FATAL) << "onComplete is not allowed on Subscription iteraction.";
}

void SubscriptionResponder::processInitialFrame(Frame_REQUEST_SUB&& frame) {
  processRequestN(frame);
}

std::ostream& SubscriptionResponder::logPrefix(std::ostream& os) {
  return os << "SubscriptionResponder(" << &connection_ << ", " << streamId_
            << "): ";
}
}
