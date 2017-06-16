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

TEST(RSocketClientServer, RacyConnect) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(port);
  auto requester = client->connect();
}

TEST(RSocketClientServer, RacyConnectLoop) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());

  for (size_t i = 0; i < 1000; ++i) {
    auto client = makeClient(port);
    auto requester = client->connect();
  }
}

// TODO: Investigate "Broken promise" errors that sporadically show up in trunk.
TEST(RSocketClientServer, DISABLED_SyncConnect) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(port);
  auto requester = client->connect().get();
}
