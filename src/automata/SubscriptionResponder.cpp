// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/automata/SubscriptionResponder.h"
#include "src/Frame.h"

namespace reactivesocket {

void SubscriptionResponder::onCompleteImpl() noexcept {
  LOG(FATAL) << "onComplete is not allowed on Subscription iteraction.";
}
}
