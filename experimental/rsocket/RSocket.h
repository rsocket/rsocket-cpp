// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketClient.h"
#include "rsocket/RSocketServer.h"


namespace rsocket {

/**
 * Main entry to creating RSocket clients and servers.
 */
class RSocket {
 public:
  /**
   * Create an RSocketClient that can be used to open RSocket connections.
   * @param connectionFactory factory of DuplexConnections on the desired
   * transport, such as TcpClientConnectionFactory
   * @return RSocketClient which can then make RSocket connections.
   */
  static std::unique_ptr<RSocketClient> createClientFactory(
      std::unique_ptr<ConnectionFactory>);

  /**
   * Create an RSocketServer that will accept connections.
   * @param connectionAcceptor acceptor of DuplexConnections on the desired
   * transport, such as TcpServerConnectionAcceptor
   * @return RSocketServer which can then accept RSocket connections.
   */
  static std::unique_ptr<RSocketServer> createServer(
      std::unique_ptr<ConnectionAcceptor> ca);

    // TODO createResumeServer

 protected:
  RSocket() = default;
};
}