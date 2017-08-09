// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include "test/handlers/HelloStreamRequestHandler.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

TEST(RSocketClientServer, StartAndShutdown) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
}

TEST(RSocketClientServer, ConnectOne) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  auto requester = client->getRequester();
}

TEST(RSocketClientServer, ConnectManySync) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(worker.getEventBase(), *server->listeningPort());
    auto requester = client->getRequester();
  }
}

TEST(RSocketClientServer, ConnectManyAsync) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(worker.getEventBase(), *server->listeningPort());
    auto requester = client->getRequester();
  }

}
