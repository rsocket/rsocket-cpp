// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include "src/DuplexConnection.h"

namespace rsocket {

/**
 * Common interface for a server that accepts connections and turns them into
 * DuplexConnection.
 *
 * This is primarily used with RSocket::createServer(ConnectionAcceptor)
 *
 * Built-in implementations can be found in rsocket/transports/, such as
 * rsocket/transports/TcpConnectionAcceptor.h
 */
class ConnectionAcceptor {
 public:
  ConnectionAcceptor() = default;
  virtual ~ConnectionAcceptor() = default;
  ConnectionAcceptor(const ConnectionAcceptor&) = delete; // copy
  ConnectionAcceptor(ConnectionAcceptor&&) = delete; // move
  ConnectionAcceptor& operator=(const ConnectionAcceptor&) = delete; // copy
  ConnectionAcceptor& operator=(ConnectionAcceptor&&) = delete; // move

  /**
   * Allocate/start required resources (threads, sockets, etc) and begin
   * listening for new connections.
   *
   * This can only be called once.
   *
   *@param onAccept
   */
  virtual void start(std::function<void(
                         std::unique_ptr<reactivesocket::DuplexConnection>,
                         folly::EventBase&)> onAccept) = 0;
  // TODO need to add numThreads option (either overload or arg with default=1)
};
}
