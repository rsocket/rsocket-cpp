// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <string>
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

using OnConnect = std::function<void(std::unique_ptr<DuplexConnection>)>;

class ClientConnectionFactory : public AsyncSocket::ConnectCallback {
 public:
  ClientConnectionFactory(std::string host, int port) : addr(host, port, true) {}

  ~ClientConnectionFactory();

  static std::unique_ptr<ClientConnectionFactory> tcpClient(
      std::string host,
      int port);

  void connect(OnConnect onConnect, ScopedEventBaseThread& eventBaseThread);

 private:
  folly::SocketAddress addr;
  folly::AsyncSocket::UniquePtr socket;
  OnConnect onConnect;

  void connectErr(const AsyncSocketException& ex) noexcept override;
  void connectSuccess() noexcept override;
};
}