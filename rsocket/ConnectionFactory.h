// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Function.h>
#include <folly/futures/Future.h>
#include "rsocket/DuplexConnection.h"

namespace folly {
class EventBase;
}

namespace rsocket {

/**
 * Common interface for a client to create connections and turn them into
 * DuplexConnections.
 *
 * This is primarily used with RSocket::createClient(ConnectionFactory)
 *
 * Built-in implementations can be found in rsocket/transports/, such as
 * rsocket/transports/TcpConnectionFactory.h
 */
class ConnectionFactory {
 public:
  ConnectionFactory() = default;
  virtual ~ConnectionFactory() = default;
  ConnectionFactory(const ConnectionFactory&) = delete; // copy
  ConnectionFactory(ConnectionFactory&&) = delete; // move
  ConnectionFactory& operator=(const ConnectionFactory&) = delete; // copy
  ConnectionFactory& operator=(ConnectionFactory&&) = delete; // move

  struct ConnectedDuplexConnection {
    std::unique_ptr<rsocket::DuplexConnection> connection;
    folly::EventBase& eventBase;
  };

  /**
   * Connect to server defined by constructor of the implementing class.
   *
   * Every time this is called a new transport connection is made. This does not
   * however mean it is a physical connection. An implementation could choose to
   * multiplex many RSocket connections on a single transport.
   *
   * Resource creation depends on the particular implementation.
   */
  virtual folly::Future<ConnectedDuplexConnection> connect() = 0;
};
} // namespace rsocket
