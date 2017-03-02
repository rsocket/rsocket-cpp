// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncSocket.h>
#include "rsocket/ConnectionFactory.h"
#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

class TcpConnectionFactory : public ConnectionFactory {
 public:
  TcpConnectionFactory(std::string host, uint16_t port);
  virtual ~TcpConnectionFactory();
  TcpConnectionFactory(const TcpConnectionFactory&) = delete; // copy
  TcpConnectionFactory(TcpConnectionFactory&&) = delete; // move
  TcpConnectionFactory& operator=(const TcpConnectionFactory&) = delete; // copy
  TcpConnectionFactory& operator=(TcpConnectionFactory&&) = delete; // move

  static std::unique_ptr<ConnectionFactory> create(std::string host, uint16_t port);

  /**
   * Connect to server on a new EventBase & Thread.
   * @param onConnect
   */
  void connect(OnConnect onConnect) override;

 private:
  folly::SocketAddress addr_;
};
}