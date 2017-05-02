// Copyright 2004-present Facebook. All Rights Reserved.

#include <random>
#include <utility>

#include <gmock/gmock.h>

#include "handlers/HelloStreamRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionAcceptor.h"
#include "rsocket/transports/TcpConnectionFactory.h"

using namespace rsocket;
using namespace rsocket::tests;

namespace {

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
  return RSocket::createClient(
      std::make_unique<TcpConnectionFactory>("localhost", port));
}

} // namespace

TEST(RSocketClientServer, StartAndShutdown) {
  makeServer(randPort());
  makeClient(randPort());
}

TEST(RSocketClientServer, SimpleConnect) {
  auto const port = randPort();
  auto server = makeServer(port);
  auto client = makeClient(port);
  auto requester = client->connect().get();
}
