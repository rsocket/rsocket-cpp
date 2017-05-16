// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/Future.h>
#include "src/ConnectionFactory.h"
#include "RSocketRequester.h"
#include "src/temporary_home/ReactiveSocket.h"

namespace rsocket {

/**
 * API for connecting to an RSocket server. Returned from RSocket::createClient.
 *
 * This connects using a transport from the provided ConnectionFactory.
 */
class RSocketClient {
 public:
  explicit RSocketClient(std::unique_ptr<ConnectionFactory>);
  ~RSocketClient(); // implementing for logging right now
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
   * - a new connection is created using a ConnectionFactory
   *
   * The returned RSocketRequester is retained by the RSocketClient instance
   * so that it lives for the life of RSocketClient, as opposed to the scope
   * of the Future (such as when used with Future.then(...)).
   *
   * To destruct a single RSocketRequester sooner than the RSocketClient
   * call RSocketRequester.close().
   */
  folly::Future<std::shared_ptr<RSocketRequester>> connect();

  // TODO implement version supporting fast start (send SETUP and requests
  // without waiting for transport to connect and ack)
  //  std::shared_ptr<RSocketRequester> fastConnect();

 private:
  std::unique_ptr<ConnectionFactory> lazyConnection_;
  std::vector<std::shared_ptr<RSocketRequester>> rsockets_;
};
}
