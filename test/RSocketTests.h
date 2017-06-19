// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <utility>

#include <gtest/gtest.h>

#include "src/RSocket.h"
#include "src/transports/tcp/TcpConnectionAcceptor.h"
#include "src/transports/tcp/TcpConnectionFactory.h"
#include "test/handlers/HelloStreamRequestHandler.h"

namespace rsocket {
namespace tests {
namespace client_server {

inline std::unique_ptr<RSocketServer> makeServer(
    std::shared_ptr<rsocket::RSocketResponder> responder) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 2;
  opts.port = 0;

  // RSocket server accepting on TCP.
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  rs->start([responder](auto& setup) { setup.createRSocket(responder); });

  return rs;
}

inline std::unique_ptr<RSocketClient> makeClient(uint16_t port) {
  folly::SocketAddress address;
  address.setFromHostPort("localhost", port);
  return RSocket::createClient(
      std::make_unique<TcpConnectionFactory>(std::move(address)));
}
}
}
} // namespace
