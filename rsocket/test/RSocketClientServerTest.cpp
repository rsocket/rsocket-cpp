// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "RSocketTests.h"

#include <folly/Random.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include "rsocket/test/handlers/HelloStreamRequestHandler.h"

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
}

TEST(RSocketClientServer, ConnectManySync) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());

  for (size_t i = 0; i < 100; ++i) {
    auto client = makeClient(worker.getEventBase(), *server->listeningPort());
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
            .thenValue(
                [&executed](std::shared_ptr<rsocket::RSocketClient> client) {
                  ++executed;
                  return client;
                })
            .thenError([&](folly::exception_wrapper ex) {
              LOG(ERROR) << "error: " << ex.what();
              ++executed;
              return std::shared_ptr<RSocketClient>(nullptr);
            });
    clients.emplace_back(std::move(clientFuture));
  }

  CHECK_EQ(clients.size(), connectionCount);
  auto results = folly::collectAll(clients).get(std::chrono::minutes{1});
  CHECK_EQ(results.size(), connectionCount);

  results.clear();
  clients.clear();
  CHECK_EQ(executed, connectionCount);
  workers.clear();
}

TEST(RSocketClientServer, ConnectOnDifferentEvb) {
  folly::ScopedEventBaseThread transportWorker{"transportWorker"};
  folly::ScopedEventBaseThread stateMachineWorker{"stateMachineWorker"};
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(
      transportWorker.getEventBase(),
      *server->listeningPort(),
      stateMachineWorker.getEventBase());
}

/// Test destroying a client with an open connection on the same worker thread
/// as that connection.
TEST(RSocketClientServer, ClientClosesOnWorker) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());

  // Move the client to the worker thread.
  worker.getEventBase()->runInEventBaseThread([c = std::move(client)] {});
}

/// Test that sending garbage to the server doesn't crash it.
TEST(RSocketClientServer, ServerGetsGarbage) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  folly::SocketAddress address{"127.0.0.1", *server->listeningPort()};

  folly::ScopedEventBaseThread worker;
  auto factory =
      std::make_shared<TcpConnectionFactory>(*worker.getEventBase(), address);

  auto result =
      factory->connect(ProtocolVersion::Latest, ResumeStatus::NEW_SESSION)
          .get();
  auto connection = std::move(result.connection);
  auto evb = &result.eventBase;

  evb->runInEventBaseThreadAndWait([conn = std::move(connection)]() mutable {
    conn->send(folly::IOBuf::copyBuffer("ABCDEFGHIJKLMNOP"));
    conn.reset();
  });
}

/// Test closing a server with a bunch of open connections.
TEST(RSocketClientServer, CloseServerWithConnections) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  std::vector<std::shared_ptr<RSocketClient>> clients;

  for (size_t i = 0; i < 100; ++i) {
    clients.push_back(
        makeClient(worker.getEventBase(), *server->listeningPort()));
  }

  server.reset();
}
