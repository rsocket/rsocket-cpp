// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/Future.h>
#include "rsocket/ConnectionFactory.h"
#include "rsocket/RSocketRequester.h"
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
  RSocketClient(const RSocketClient&) = delete; // copy
  RSocketClient(RSocketClient&&) = delete; // move
  RSocketClient& operator=(const RSocketClient&) = delete; // copy
  RSocketClient& operator=(RSocketClient&&) = delete; // move

  // TODO ConnectionSetupPayload
  // TODO keepalive timer
  // TODO duplex with RequestHandler

  /*
   * Connect asynchronously and return a Future which will deliver the RSocket
   *
   * Each time this is called:
   * - a new thread and EventBase is created
   * - a new connection is created
   * - a new StandardReactiveSocket is created
   */
  Future<std::shared_ptr<RSocketRequester>> connect();

  /*
   * Connect asynchronously and return a Future which will deliver the RSocket
   *
   * Each time this is called:
   * - a new StandardReactiveSocket is created using the provided EventBase
   */
  // TODO implement overload that allows injecting an EventBase
  //  Future<std::shared_ptr<StandardReactiveSocket>> connect(Executor&);

  // TODO implement version supporting fast start (send SETUP and requests
  // without waiting for transport to connect and ack)
  //  std::shared_ptr<StandardReactiveSocket> fastConnect();
  //  std::shared_ptr<StandardReactiveSocket> fastConnect(Executor&);

 private:
  std::unique_ptr<ConnectionFactory> lazyConnection;
  std::vector<std::shared_ptr<RSocketRequester>> rsockets_;
};
}