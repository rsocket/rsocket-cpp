// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/ConnectionAcceptor.h"
#include "src/RequestHandler.h"
#include "src/ServerConnectionAcceptor.h"
#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

namespace rsocket {

/**
 * Represents a new connection request from a client,
 * either a SETUP or RESUME frame.
 */
class ConnectionRequest {
 public:
  ConnectionRequest(ConnectionSetupPayload setupPayload);
  bool isResumptionAttempt();

 private:
  ConnectionSetupPayload setupPayload_;
};
}