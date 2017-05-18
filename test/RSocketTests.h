// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <random>
#include <utility>

#include <gtest/gtest.h>

#include "src/RSocket.h"
#include "src/transports/tcp/TcpConnectionAcceptor.h"
#include "src/transports/tcp/TcpConnectionFactory.h"
#include "test/handlers/HelloStreamRequestHandler.h"

namespace rsocket {
namespace tests {
namespace client_server {

// Helps prevent against port collisions.
inline uint16_t randPort() {
  std::random_device device;
  std::uniform_int_distribution<uint16_t> dis(9000, 65000);

  auto const n = dis(device);
  return static_cast<uint16_t>(n);
}

inline std::unique_ptr<RSocketServer> makeServer(
    uint16_t port,
    std::shared_ptr<rsocket::RSocketResponder> responder) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 2;
  opts.port = port;

  // RSocket server accepting on TCP.
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  rs->start([responder](auto r) { return responder; });

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