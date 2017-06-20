// Copyright 2004-present Facebook. All Rights Reserved.

#include <random>
#include <utility>

#include "RSocketTests.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

TEST(RSocketClientServer, StartAndShutdown) {
  makeServer(randPort(), std::make_shared<HelloStreamRequestHandler>());
  makeClient(randPort());
}

TEST(RSocketClientServer, ConnectOne) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(port);
  auto requester = client->connect().get();
}

TEST(RSocketClientServer, ConnectManySync) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(port);
    auto requester = client->connect().get();
  }
}

// TODO: Investigate why this hangs (even with i < 2).
TEST(RSocketClientServer, DISABLED_ConnectManyAsync) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());

  std::vector<folly::Future<folly::Unit>> futures;

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(port);
    auto requester = client->connect();

    futures.push_back(requester.unit());
  }

  folly::collectAll(futures).get();
}
