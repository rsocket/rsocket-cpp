// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncTransport.h>

#include "rsocket/ConnectionFactory.h"
#include "rsocket/DuplexConnection.h"

namespace rsocket {

class RSocketStats;

/**
 * TCP implementation of ConnectionFactory for use with RSocket::createClient().
 *
 * Creation of this does nothing.  The `start` method kicks off work.
 */
class TcpConnectionFactory : public ConnectionFactory {
 public:
  TcpConnectionFactory(
      folly::EventBase& eventBase,
      folly::SocketAddress,
      bool batchIo = false);
  virtual ~TcpConnectionFactory();

  /**
   * Connect to server defined in constructor.
   *
   * Each call to connect() creates a new AsyncSocket.
   */
  folly::Future<ConnectedDuplexConnection> connect() override;

  static std::unique_ptr<DuplexConnection> createDuplexConnectionFromSocket(
      folly::AsyncTransportWrapper::UniquePtr socket,
      bool batchIo,
      std::shared_ptr<RSocketStats> stats = std::shared_ptr<RSocketStats>());

 private:
  folly::SocketAddress address_;
  folly::EventBase* eventBase_;
  bool batchIo_;
};
} // namespace rsocket
