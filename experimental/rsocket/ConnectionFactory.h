// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace folly {
class EventBase;
}

namespace rsocket {

using OnConnect =
    std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>;

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

  /**
   * Connect to server defined by constructor of the implementing class.
   *
   * Everytime this is called a new connection is made.
   * @param onConnect
   */
  virtual void connect(OnConnect onConnect) = 0;
};
}