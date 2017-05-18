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

// TODO(alexanderm): Failing upon closing the server.  Error says we're on the
// wrong EventBase for the AsyncSocket.
TEST(RSocketClientServer, DISABLED_SimpleConnect) {
  auto const port = randPort();
  auto server = makeServer(port, std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(port);
  auto requester = client->connect().get();
}
