// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

TEST(RSocketClientServer, StartAndShutdown) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(*server->listeningPort());
}

TEST(RSocketClientServer, InvalidPort) {
  // Port 1 should always be invalid.
  TcpConnectionAcceptor::Options opts;
  opts.port = 1;

  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  try {
    rs->start([](auto& setup) {
      setup.createRSocket(std::make_shared<HelloStreamRequestHandler>());
    });
    FAIL();
  } catch (const std::exception& exn) {
    LOG(INFO) << "Caught error: " << exn.what();
  }
}

TEST(RSocketClientServer, ConnectOne) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(*server->listeningPort());
  auto requester = client->connect().get();
}

TEST(RSocketClientServer, ConnectManySync) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(*server->listeningPort());
    auto requester = client->connect().get();
  }
}

TEST(RSocketClientServer, ConnectManyAsync) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  std::vector<std::unique_ptr<RSocketClient>> clients;
  std::vector<folly::Future<folly::Unit>> futures;

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(*server->listeningPort());
    auto requester = client->connect();

    clients.push_back(std::move(client));
    futures.push_back(requester.unit());
  }

  folly::collectAll(futures).get();
}
