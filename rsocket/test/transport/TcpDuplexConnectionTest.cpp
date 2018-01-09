// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/futures/Future.h>
#include <folly/io/async/AsyncTransport.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>

#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "rsocket/test/transport/DuplexConnectionTest.h"

namespace rsocket {
namespace tests {

using namespace folly;
using namespace rsocket;
using namespace ::testing;

/**
 * Synchronously create a server and a client.
 */
std::pair<
    std::unique_ptr<ConnectionAcceptor>,
    std::unique_ptr<ConnectionFactory>>
makeSingleClientServer(
    std::unique_ptr<DuplexConnection>& serverConnection,
    EventBase** serverEvb,
    std::unique_ptr<DuplexConnection>& clientConnection,
    EventBase* clientEvb) {
  Promise<Unit> serverPromise;

  TcpConnectionAcceptor::Options options(
      0 /*port*/, 1 /*threads*/, 0 /*backlog*/);
  auto server = std::make_unique<TcpConnectionAcceptor>(options);
  server->start(
      [&serverPromise, &serverConnection, &serverEvb](
          std::unique_ptr<DuplexConnection> connection, EventBase& eventBase) {
        serverConnection = std::move(connection);
        *serverEvb = &eventBase;
        serverPromise.setValue();
      });

  int16_t port = server->listeningPort().value();

  auto client = std::make_unique<TcpConnectionFactory>(
      *clientEvb,
      SocketAddress("localhost", port, true));
  client->connect().then(
      [&clientConnection](
          ConnectionFactory::ConnectedDuplexConnection connection) {
        clientConnection = std::move(connection.connection);
      }).wait();

  serverPromise.getFuture().wait();
  return std::make_pair(std::move(server), std::move(client));
}

TEST(TcpDuplexConnection, MultipleSetInputGetOutputCalls) {
  folly::ScopedEventBaseThread worker;
  std::unique_ptr<DuplexConnection> serverConnection, clientConnection;
  EventBase *serverEvb = nullptr;
  auto keepAlive = makeSingleClientServer(
      serverConnection, &serverEvb, clientConnection, worker.getEventBase());
  makeMultipleSetInputGetOutputCalls(
      std::move(serverConnection),
      serverEvb,
      std::move(clientConnection),
      worker.getEventBase());
}

TEST(TcpDuplexConnection, InputAndOutputIsUntied) {
  folly::ScopedEventBaseThread worker;
  std::unique_ptr<DuplexConnection> serverConnection, clientConnection;
  EventBase *serverEvb = nullptr;
  auto keepAlive = makeSingleClientServer(
      serverConnection, &serverEvb, clientConnection, worker.getEventBase());
  verifyInputAndOutputIsUntied(
      std::move(serverConnection),
      serverEvb,
      std::move(clientConnection),
      worker.getEventBase());
}

TEST(TcpDuplexConnection, ConnectionAndSubscribersAreUntied) {
  folly::ScopedEventBaseThread worker;
  std::unique_ptr<DuplexConnection> serverConnection, clientConnection;
  EventBase *serverEvb = nullptr;
  auto keepAlive = makeSingleClientServer(
      serverConnection, &serverEvb, clientConnection, worker.getEventBase());
  verifyClosingInputAndOutputDoesntCloseConnection(
      std::move(serverConnection),
      serverEvb,
      std::move(clientConnection),
      worker.getEventBase());
}

} // namespace tests
} // namespace rsocket
