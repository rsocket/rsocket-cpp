// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/ConnectionRequest.h"

using namespace ::reactivesocket;

namespace rsocket {

ConnectionRequest::ConnectionRequest(ConnectionSetupPayload setupPayload)
    : setupPayload_(std::move(setupPayload)) {}

bool ConnectionRequest::isResumptionAttempt() {
  return false;
}
}