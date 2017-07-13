// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/RSocketClient.h"
#include "src/RSocketServer.h"

namespace rsocket {

/**
 * Main entry to creating RSocket clients and servers.
 */
class RSocket {
 public:
  /**
   * Create an RSocketClient that can be used to open RSocket connections.
   * Takes a factory of DuplexConnections on the desired transport, such as
   * TcpClientConnectionFactory.
   */
  static std::unique_ptr<RSocketClient> createClient(
      std::unique_ptr<ConnectionFactory>);

  // TODO duplex client that takes a requestHandler
  // TODO ConnectionSetupPayload arguments such as MimeTypes, Keepalive, etc

  /**
   * Create an RSocketServer that will accept connections.  Takes an acceptor of
   * DuplexConnections on the desired transport, such as
   * TcpServerConnectionAcceptor
   */
  static std::unique_ptr<RSocketServer> createServer(
      std::unique_ptr<ConnectionAcceptor>);

  // TODO createResumeServer

  RSocket() = delete;

  RSocket(const RSocket&) = delete;
  RSocket(RSocket&&) = delete;

  RSocket& operator=(const RSocket&) = delete;
  RSocket& operator=(RSocket&&) = delete;
};
}
