// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

#include <folly/Random.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include "rsocket/transports/tcp/TcpDuplexConnection.h"
#include "test/handlers/HelloStreamRequestHandler.h"

using namespace folly::io;
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
            .then([&executed](std::shared_ptr<rsocket::RSocketClient> client) {
              ++executed;
              return client;
            })
            .onError([&](folly::exception_wrapper ex) {
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

/// Test destroying a client with an open connection on the same worker thread
/// as that connection.
TEST(RSocketClientServer, ClientClosesOnWorker) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());

  // Move the client to the worker thread.
  worker.getEventBase()->runInEventBaseThread([c = std::move(client)]{});
}

/// Test that sending garbage to the server doesn't crash it.
TEST(RSocketClientServer, ServerGetsGarbage) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  folly::SocketAddress address{"::1", *server->listeningPort()};

  folly::ScopedEventBaseThread worker;
  auto factory =
      std::make_shared<TcpConnectionFactory>(*worker.getEventBase(), address);

  auto result = factory->connect().get();
  auto connection = std::move(result.connection);
  auto evb = &result.eventBase;

  evb->runInEventBaseThreadAndWait([conn = std::move(connection)]() mutable {
    auto output = conn->getOutput();
    output->onSubscribe(yarpl::flowable::Subscription::empty());
    output->onNext(folly::IOBuf::copyBuffer("ABCDEFGHIJKLMNOP"));
    output->onComplete();
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

class SocketCallback : public folly::AsyncServerSocket::AcceptCallback {
 public:
  void connectionAccepted(
      int fd,
      const folly::SocketAddress& address) noexcept override {
    VLOG(2) << "Accepting TCP connection from " << address << " on FD " << fd;

    socketPromise.setValue(fd);
  }

  void acceptError(const std::exception& ex) noexcept override {
    VLOG(2) << "TCP error: " << ex.what();

    socketPromise.setException(ex);
  }

  folly::Future<int> getConnectedSocket() {
    return socketPromise.getFuture();
  }

 protected:
  folly::Promise<int> socketPromise;
};

class RSocketClientServerRaw : public ::testing::TestWithParam<bool> {};

TEST_P(RSocketClientServerRaw, ServerWithNoAcceptor) {
  // ConnectionAcceptor's should destroy the threads the last!
  auto connectedClientThread = std::make_unique<folly::ScopedEventBaseThread>();
  // Callbacks are also required to be destroyed the last!
  auto callback = std::make_unique<SocketCallback>();
  {
    folly::ScopedEventBaseThread serverThread;
    auto serverSocket = folly::AsyncServerSocket::UniquePtr(
        new folly::AsyncServerSocket(serverThread.getEventBase()));

    auto connectedClientEvb = connectedClientThread->getEventBase();
    auto serverPort =
        folly::via(
            serverThread.getEventBase(),
            [&serverSocket, &serverThread, &callback, connectedClientEvb]()
                -> unsigned short {
              serverSocket->bind(folly::SocketAddress("::", 0));
              serverSocket->addAcceptCallback(
                  callback.get(), connectedClientEvb);
              serverSocket->listen(/*backlog=*/1);
              serverSocket->startAccepting();
              for (auto& i : serverSocket->getAddresses()) {
                return i.getPort();
              }
              LOG(FATAL) << "Server could not be start";
            })
            .get();

    // Thread should outlive the client.
    auto clientThread = std::make_unique<folly::ScopedEventBaseThread>();
    {
      // Client connects to the server.
      auto client = makeClient(clientThread->getEventBase(), serverPort);

      // Server should have a connection now.
      auto fd = callback->getConnectedSocket().get();

      // Make a server with no Acceptor as we will already provide the
      // connection!
      auto server = makeServerWithNoAcceptor();

      connectedClientEvb->runInEventBaseThreadAndWait([&server,
                                                       &connectedClientEvb,
                                                       fd]() {
        folly::AsyncSocket::UniquePtr socket(
            new folly::AsyncSocket(connectedClientEvb, fd));

        auto connection =
            std::make_unique<TcpDuplexConnection>(std::move(socket));

        folly::EventBase dummyEventBase;
        server->acceptConnection(
            std::move(connection),
            dummyEventBase,
            RSocketServiceHandler::create([](const rsocket::SetupParameters&) {
              return std::make_shared<HelloStreamRequestHandler>();
            }));
      });

      auto disconnect = GetParam();
      if (disconnect) {
        client->disconnect().wait();
      }
    }

    folly::Baton<> baton;
    serverThread.getEventBase()->runInEventBaseThread(
        [ serverSocket = std::move(serverSocket), &baton ]() {
          serverSocket->stopAccepting();
          baton.post();
        });
    baton.wait();
  }
}

TEST_P(RSocketClientServerRaw, ClientFromExistingConnection) {
  auto server = makeServer(std::make_shared<HelloStreamRequestHandler>());
  // The eventBase should outlive the RSocketClient instance. Otherwise it
  // causes leak that is caught on ASAN builds.
  folly::EventBase evb;
  {
    std::unique_ptr<RSocketClient> client;
    {
      folly::SocketAddress address{"::1", *server->listeningPort()};
      folly::AsyncSocket::UniquePtr socket(
          new folly::AsyncSocket(&evb, address));
      client = makeClientFromConnection(std::move(socket), evb);

      bool disconnect = GetParam();
      if (disconnect) {
        client->disconnect().wait();
      }
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    Disconnect,
    RSocketClientServerRaw,
    ::testing::Values(true, false));
