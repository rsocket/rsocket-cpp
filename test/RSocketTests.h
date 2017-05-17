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

std::random_device device;
std::uniform_int_distribution<uint16_t> dis(9000, 10000);

// Helps prevent against port collisions.
uint16_t randPort() {
  auto const n = dis(device);
  return static_cast<uint16_t>(n);
}

std::unique_ptr<RSocketServer> makeServer(uint16_t port) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 2;
  opts.port = port;

  // RSocket server accepting on TCP.
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  // Global request handler.
  auto handler = std::make_shared<HelloStreamRequestHandler>();

  rs->start([handler](auto r) { return handler; });

  return rs;
}

std::unique_ptr<RSocketClient> makeClient(uint16_t port) {
  folly::SocketAddress address;
  address.setFromHostPort("localhost", port);
  return RSocket::createClient(
      std::make_unique<TcpConnectionFactory>(std::move(address)));
}
}
}
} // namespace