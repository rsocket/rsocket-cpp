// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include <gtest/gtest.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "test/handlers/HelloStreamRequestHandler.h"

namespace rsocket {
namespace tests {
namespace client_server {

inline std::unique_ptr<RSocketServer> makeServer(
    std::shared_ptr<rsocket::RSocketResponder> responder) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 2;
  opts.address = folly::SocketAddress("::", 0);

  // RSocket server accepting on TCP.
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  rs->start([r = std::move(responder)](const SetupParameters&) { return r; });

  return rs;
}

inline std::unique_ptr<RSocketServer> makeResumableServer(
    std::shared_ptr<RSocketServiceHandler> serviceHandler) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 1;
  opts.address = folly::SocketAddress("::", 0);
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  rs->start(std::move(serviceHandler));
  return rs;
}

inline std::shared_ptr<RSocketClient> makeClient(uint16_t port) {
  folly::SocketAddress address;
  address.setFromHostPort("localhost", port);
  return RSocket::createConnectedClient(
             std::make_unique<TcpConnectionFactory>(std::move(address)))
      .get();
}

inline std::shared_ptr<RSocketClient> makeResumableClient(uint16_t port) {
  folly::SocketAddress address;
  address.setFromHostPort("localhost", port);
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  return RSocket::createConnectedClient(
             std::make_unique<TcpConnectionFactory>(std::move(address)),
             std::move(setupParameters))
      .get();
}
}
}
} // namespace
