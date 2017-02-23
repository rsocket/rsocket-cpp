// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/Future.h>
#include "rsocket/ConnectionFactory.h"
#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

/**
 * Simplified API for client/server
 */
namespace rsocket {

class RSocketClient {
 public:
  RSocketClient(std::unique_ptr<ConnectionFactory>);
  ~RSocketClient();

    // TODO ConnectionSetupPayload
    // TODO keepalive timer
    // TODO duplex with RequestHandler
    // TODO constructor with normal EventBase, not just ScopedEventBaseThread

  /*
 * Connect asynchronously and return a Future
 * which will deliver the RSocket
 */
  Future<std::shared_ptr<StandardReactiveSocket>> connect(
      ScopedEventBaseThread& eventBaseThread);

 private:
  std::unique_ptr<ConnectionFactory> lazyConnection;
  std::shared_ptr<StandardReactiveSocket> rs;
};
}