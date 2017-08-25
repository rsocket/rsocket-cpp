// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

#include <folly/Random.h>
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
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  constexpr size_t connectionCount = 100;
  constexpr size_t workerCount = 10;
  std::vector<folly::ScopedEventBaseThread> workers(workerCount);
  std::vector<folly::Future<std::shared_ptr<RSocketClient>>> clients;

  std::atomic<int> executed{0};
  for (size_t i = 0; i < connectionCount; ++i) {
    int workerId = folly::Random::rand32(workerCount);
    auto clientFuture =
        makeClientAsync(
            workers[workerId].getEventBase(), *server->listeningPort())
            .then([&executed](std::shared_ptr<rsocket::RSocketClient> client) {
              auto requester = client->getRequester();
              client->disconnect(folly::exception_wrapper());
              ++executed;
              return client;
            });
    clients.emplace_back(std::move(clientFuture));
  }

  auto results = folly::collectAll(clients).get(folly::Duration(5000));
  results.clear();
  clients.clear();
  CHECK_EQ(executed, connectionCount);
  workers.clear();
}
