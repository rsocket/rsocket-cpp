// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncSocket.h>
#include "rsocket/ConnectionFactory.h"
#include "src/DuplexConnection.h"

namespace rsocket {

/**
* TCP implementation of ConnectionAcceptor for use with RSocket::createServer
*
* Creation of this does nothing. The 'start' method kicks off work.
*
* When started it will create a Thread, EventBase, and AsyncSocket.
*/
class TcpConnectionFactory : public ConnectionFactory {
 public:
  TcpConnectionFactory(std::string host, uint16_t port);
  // TODO create variant that passes in Thread/EventBase to use
  virtual ~TcpConnectionFactory();
  TcpConnectionFactory(const TcpConnectionFactory&) = delete; // copy
  TcpConnectionFactory(TcpConnectionFactory&&) = delete; // move
  TcpConnectionFactory& operator=(const TcpConnectionFactory&) = delete; // copy
  TcpConnectionFactory& operator=(TcpConnectionFactory&&) = delete; // move

  static std::unique_ptr<ConnectionFactory> create(
      std::string host,
      uint16_t port);

  /**
   * Connect to server defined in constructor.
   *
   * Each time this is called a new AsyncSocket is created and connected.
   *
   * This creates a new AsyncSocket each time connect(...) is called.
   *
   * @param onConnect
   */
  void connect(OnConnect onConnect) override;

 private:
  folly::SocketAddress addr_;
};
}
