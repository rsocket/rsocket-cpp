// Copyright 2004-present Facebook. All Rights Reserved.

#include "benchmarks/Fixture.h"

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

namespace rsocket {

namespace {

std::shared_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    folly::SocketAddress address) {
  auto factory =
      std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address));
  return RSocket::createConnectedClient(std::move(factory)).get();
}
}

Fixture::Fixture(
    Fixture::Options fixtureOpts,
    std::shared_ptr<RSocketResponder> responder)
    : options{std::move(fixtureOpts)} {
  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress{"::1", 0};
  opts.threads = options.serverThreads;

  auto acceptor = std::make_unique<TcpConnectionAcceptor>(std::move(opts));
  server = std::make_unique<RSocketServer>(std::move(acceptor));
  server->start([responder](const SetupParameters&) { return responder; });

  auto const numWorkers =
      options.clientThreads ? *options.clientThreads : options.clients;
  for (size_t i = 0; i < numWorkers; ++i) {
    workers.push_back(std::make_unique<folly::ScopedEventBaseThread>(
        "rsocket-client-thread"));
  }

  const folly::SocketAddress actual{"::1", *server->listeningPort()};

  for (size_t i = 0; i < options.clients; ++i) {
    auto worker = std::move(workers.front());
    workers.pop_front();
    clients.push_back(makeClient(worker->getEventBase(), actual));
    workers.push_back(std::move(worker));
  }
}
}
