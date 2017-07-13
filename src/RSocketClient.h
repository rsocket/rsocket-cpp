// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/Future.h>

#include "src/ConnectionFactory.h"
#include "src/RSocketNetworkStats.h"
#include "src/RSocketParameters.h"
#include "src/RSocketRequester.h"
#include "src/RSocketStats.h"

namespace rsocket {

class RSocketConnectionManager;

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
  folly::Future<std::unique_ptr<RSocketRequester>> connect(
      SetupParameters setupParameters = SetupParameters(),
      std::shared_ptr<RSocketResponder> responder = std::shared_ptr<RSocketResponder>(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer = std::unique_ptr<KeepaliveTimer>(),
      std::shared_ptr<RSocketStats> stats = std::shared_ptr<RSocketStats>(),
      std::shared_ptr<RSocketNetworkStats> networkStats = std::shared_ptr<RSocketNetworkStats>());

  // TODO implement version supporting fast start (send SETUP and requests
  // without waiting for transport to connect and ack)
  //  std::shared_ptr<RSocketRequester> fastConnect();

  std::unique_ptr<RSocketRequester> fromConnection(
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase& eventBase,
      SetupParameters setupParameters = SetupParameters(),
      std::shared_ptr<RSocketResponder> responder = std::shared_ptr<RSocketResponder>(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer = std::unique_ptr<KeepaliveTimer>(),
      std::shared_ptr<RSocketStats> stats = std::shared_ptr<RSocketStats>(),
      std::shared_ptr<RSocketNetworkStats> networkStats = std::shared_ptr<RSocketNetworkStats>());

 private:
  std::unique_ptr<ConnectionFactory> connectionFactory_;
  std::unique_ptr<RSocketConnectionManager> connectionManager_;
};
}
