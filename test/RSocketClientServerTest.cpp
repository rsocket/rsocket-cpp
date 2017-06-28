// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

TEST(RSocketClientServer, StartAndShutdown) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(*server->listeningPort());
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

// TODO: Investigate why this hangs (even with i < 2).
TEST(RSocketClientServer, DISABLED_ConnectManyAsync) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  std::vector<folly::Future<folly::Unit>> futures;

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(*server->listeningPort());
    auto requester = client->connect();

    futures.push_back(requester.unit());
  }

  folly::collectAll(futures).get();
}
