// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Optional.h>

#include "rsocket/DuplexConnection.h"

namespace folly {
class EventBase;
}

namespace rsocket {

using OnDuplexConnectionAccept = std::function<void(
    std::unique_ptr<rsocket::DuplexConnection>,
    folly::EventBase&)>;

/**
 * Common interface for a server that accepts connections and turns them into
 * DuplexConnection.
 *
 * This is primarily used with RSocket::createServer(ConnectionAcceptor)
 *
 * Built-in implementations can be found in rsocket/transports/, such as
 * rsocket/transports/TcpConnectionAcceptor.h
 *
 * TODO: Add way of specifying number of worker threads.
 */
class ConnectionAcceptor {
 public:
  ConnectionAcceptor() = default;
  virtual ~ConnectionAcceptor() = default;

  ConnectionAcceptor(const ConnectionAcceptor&) = delete;
  ConnectionAcceptor(ConnectionAcceptor&&) = delete;

  ConnectionAcceptor& operator=(const ConnectionAcceptor&) = delete;
  ConnectionAcceptor& operator=(ConnectionAcceptor&&) = delete;

  /**
   * Allocate/start required resources (threads, sockets, etc) and begin
   * listening for new connections.  Must be synchronous.
   *
   * This can only be called once.
   */
  virtual void start(OnDuplexConnectionAccept) = 0;

  /**
   * Stop listening for new connections.
   *
   * This can only be called once.  Must be called in or before
   * the implementation's destructor.  Must be synchronous.
   */
  virtual void stop() = 0;

  /**
   * Get the port the acceptor is listening on.  Returns folly::none when the
   * acceptor is not listening.
   */
  virtual folly::Optional<uint16_t> listeningPort() const = 0;
};
} // namespace rsocket
