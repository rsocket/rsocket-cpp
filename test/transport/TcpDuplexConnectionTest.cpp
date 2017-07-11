// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/futures/Future.h>
#include <folly/io/async/AsyncSocket.h>
#include <gtest/gtest.h>

#include "src/transports/tcp/TcpConnectionAcceptor.h"
#include "src/transports/tcp/TcpConnectionFactory.h"
#include "test/transport/DuplexConnectionTest.h"

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
    EventBase** clientEvb) {
  Promise<Unit> serverPromise, clientPromise;

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
      SocketAddress("localhost", port, true));
  client->connect(
      [&clientPromise, &clientConnection, &clientEvb](
          std::unique_ptr<DuplexConnection> connection, EventBase& eventBase) {
        clientConnection = std::move(connection);
        *clientEvb = &eventBase;
        clientPromise.setValue();
      });

  serverPromise.getFuture().wait();
  clientPromise.getFuture().wait();
  return std::make_pair(std::move(server), std::move(client));
}

TEST(TcpDuplexConnection, MultipleSetInputGetOutputCalls) {
  std::unique_ptr<DuplexConnection> serverConnection, clientConnection;
  EventBase *serverEvb = nullptr, *clientEvb = nullptr;
  auto keepAlive = makeSingleClientServer(
      serverConnection, &serverEvb, clientConnection, &clientEvb);
  makeMultipleSetInputGetOutputCalls(
      std::move(serverConnection),
      serverEvb,
      std::move(clientConnection),
      clientEvb);
}

TEST(TcpDuplexConnection, InputAndOutputIsUntied) {
  std::unique_ptr<DuplexConnection> serverConnection, clientConnection;
  EventBase *serverEvb = nullptr, *clientEvb = nullptr;
  auto keepAlive = makeSingleClientServer(
      serverConnection, &serverEvb, clientConnection, &clientEvb);
  verifyInputAndOutputIsUntied(
      std::move(serverConnection),
      serverEvb,
      std::move(clientConnection),
      clientEvb);
}

TEST(TcpDuplexConnection, ConnectionAndSubscribersAreUntied) {
  std::unique_ptr<DuplexConnection> serverConnection, clientConnection;
  EventBase *serverEvb = nullptr, *clientEvb = nullptr;
  auto keepAlive = makeSingleClientServer(
      serverConnection, &serverEvb, clientConnection, &clientEvb);
  verifyClosingInputAndOutputDoesntCloseConnection(
      std::move(serverConnection),
      serverEvb,
      std::move(clientConnection),
      clientEvb);
}

} // namespace tests
} // namespace rsocket
