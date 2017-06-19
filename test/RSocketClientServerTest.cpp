// Copyright 2004-present Facebook. All Rights Reserved.

#include <random>
#include <utility>

#include "RSocketTests.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

TEST(RSocketClientServer, StartAndShutdown) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(*server->listeningPort());
}

// TODO(alexanderm): Failing upon closing the server.  Error says we're on the
// wrong EventBase for the AsyncSocket.
TEST(RSocketClientServer, DISABLED_SimpleConnect) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(*server->listeningPort());
  auto requester = client->connect().get();
}
