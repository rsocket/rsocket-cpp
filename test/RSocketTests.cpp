// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/RSocketTests.h"

#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

namespace rsocket {
namespace tests {
namespace client_server {

std::unique_ptr<RSocketServer> makeServer(
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

std::unique_ptr<RSocketServer> makeResumableServer(
    std::shared_ptr<RSocketServiceHandler> serviceHandler) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 1;
  opts.address = folly::SocketAddress("::", 0);
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  rs->start(std::move(serviceHandler));
  return rs;
}

std::shared_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    uint16_t port) {
  CHECK(eventBase);

  folly::SocketAddress address;
  address.setFromHostPort("localhost", port);
  return RSocket::createConnectedClient(
      std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address)))
      .get();
}

std::shared_ptr<RSocketClient> makeResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents) {
  CHECK(eventBase);

  folly::SocketAddress address;
  address.setFromHostPort("localhost", port);
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  return RSocket::createConnectedClient(
      std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address)),
      std::move(setupParameters),
      std::make_shared<RSocketResponder>(),
      nullptr,
      RSocketStats::noop(),
      std::move(connectionEvents))
      .get();
}

} // namespace client_server
} // namespace tests
} // namespace rsocket
