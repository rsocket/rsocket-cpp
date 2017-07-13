// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/SocketAddress.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include "src/ConnectionFactory.h"
#include "src/DuplexConnection.h"

namespace rsocket {

class RSocketStats;

/**
 * TCP implementation of ConnectionFactory for use with RSocket::createClient().
 *
 * Creation of this does nothing.  The `start` method kicks off work.
 */
class TcpConnectionFactory : public ConnectionFactory {
 public:
  explicit TcpConnectionFactory(folly::SocketAddress);
  virtual ~TcpConnectionFactory();

  /**
   * Connect to server defined in constructor.
   *
   * Each call to connect() creates a new AsyncSocket.
   */
  void connect(OnDuplexConnectionConnect) override;

  static std::unique_ptr<DuplexConnection> createDuplexConnectionFromSocket(
      folly::AsyncSocket::UniquePtr socket,
      std::shared_ptr<RSocketStats> stats = std::shared_ptr<RSocketStats>());

 private:
  folly::SocketAddress address_;
  folly::ScopedEventBaseThread worker_;
};
} // namespace rsocket
