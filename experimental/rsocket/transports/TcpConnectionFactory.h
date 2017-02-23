// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include "rsocket/ConnectionFactory.h"
#include "src/DuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

using OnConnect = std::function<void(std::unique_ptr<DuplexConnection>)>;

class TcpConnectionFactory : public ConnectionFactory,
                                   public AsyncSocket::ConnectCallback {
 public:
  TcpConnectionFactory(std::string host, int port)
      : addr(host, port, true) {}

  ~TcpConnectionFactory();

  static std::unique_ptr<ConnectionFactory> create(
          std::string host,
          int port);

  void connect(OnConnect onConnect, ScopedEventBaseThread& eventBaseThread)
      override;

 private:
  folly::SocketAddress addr;
  folly::AsyncSocket::UniquePtr socket;
  OnConnect onConnect;

  void connectErr(const AsyncSocketException& ex) noexcept override;
  void connectSuccess() noexcept override;
};
}