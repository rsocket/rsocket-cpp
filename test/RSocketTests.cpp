// Copyright 2004-present Facebook. All Rights Reserved.

#include "test/RSocketTests.h"

#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "test/test_utils/GenericRequestResponseHandler.h"

namespace rsocket {
namespace tests {
namespace client_server {

std::unique_ptr<TcpConnectionFactory> getConnFactory(
    folly::EventBase* eventBase,
    uint16_t port) {
  folly::SocketAddress address{"::1", port};
  return std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address));
}

std::unique_ptr<RSocketServer> makeServer(
    std::shared_ptr<rsocket::RSocketResponder> responder) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 2;
  opts.address = folly::SocketAddress("::", 0);

  // RSocket server accepting on TCP.
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  rs->start([r = std::move(responder)](const SetupParameters&) { return r; });
  return rs;
}

std::unique_ptr<RSocketServer> makeResumableServer(
    std::shared_ptr<RSocketServiceHandler> serviceHandler) {
  TcpConnectionAcceptor::Options opts;
  opts.threads = 3;
  opts.address = folly::SocketAddress("::", 0);
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  rs->start(std::move(serviceHandler));
  return rs;
}

// This server will have no Acceptor, so we will provide the connections
// to this server. Here is an example:
//
// folly::AsyncSocket::UniquePtr sock : <given as parameter>
// shared_ptr<RSocketServiceHandler> serviceHandler : <given as parameter>
//
// auto connection = std::make_unique<TcpDuplexConnection>(std::move(socket));
//
// server->acceptConnection(
//    std::move(connection), dummyEventBase, serviceHandler);
//
std::unique_ptr<RSocketServer> makeServerWithNoAcceptor() {
  // Note that we have no Acceptor but we still have all rest in the created
  // server, like ConnectionManager instance.
  return RSocket::createServer(nullptr);
}

folly::Future<std::unique_ptr<RSocketClient>> makeClientAsync(
    folly::EventBase* eventBase,
    uint16_t port) {
  CHECK(eventBase);
  return RSocket::createConnectedClient(getConnFactory(eventBase, port));
}

// If we already have a connection, we can utilize it.
folly::Future<std::unique_ptr<RSocketClient>> makeClientFromConnectionAsync(
    folly::AsyncSocket::UniquePtr socket,
    folly::EventBase& eventBase) {
  auto dupCon =
      TcpConnectionFactory::createDuplexConnectionFromSocket(std::move(socket));
  return RSocket::createClientFromConnection(std::move(dupCon), eventBase);
}

std::unique_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    uint16_t port) {
  return makeClientAsync(eventBase, port).get();
}

std::unique_ptr<RSocketClient> makeClientFromConnection(
    folly::AsyncSocket::UniquePtr socket,
    folly::EventBase& eventBase) {
  return makeClientFromConnectionAsync(std::move(socket), eventBase).get();
}

namespace {
struct DisconnectedResponder : public rsocket::RSocketResponder {
  yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestStream(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  DisconnectedResponder() {}

  yarpl::Reference<yarpl::single::Single<rsocket::Payload>>
  handleRequestResponse(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestChannel(
      rsocket::Payload,
      yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>,
      rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  void handleFireAndForget(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
  }

  void handleMetadataPush(std::unique_ptr<folly::IOBuf>) override {
    CHECK(false);
  }

  ~DisconnectedResponder() {}
};
} // namespace

std::unique_ptr<RSocketClient> makeDisconnectedClient(
    folly::EventBase* eventBase) {
  auto server = makeServer(std::make_shared<DisconnectedResponder>());

  auto client = makeClient(eventBase, *server->listeningPort());
  client->disconnect().get();
  return client;
}

std::unique_ptr<RSocketClient> makeWarmResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents) {
  CHECK(eventBase);
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  return RSocket::createConnectedClient(
             getConnFactory(eventBase, port),
             std::move(setupParameters),
             std::make_shared<RSocketResponder>(),
             nullptr,
             RSocketStats::noop(),
             std::move(connectionEvents))
      .get();
}

std::unique_ptr<RSocketClient> makeColdResumableClient(
    folly::EventBase* eventBase,
    uint16_t port,
    ResumeIdentificationToken token,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler) {
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  setupParameters.token = token;
  return RSocket::createConnectedClient(
             getConnFactory(eventBase, port),
             std::move(setupParameters),
             nullptr, // responder
             nullptr, // keepAliveTimer
             nullptr, // stats
             nullptr, // connectionEvents
             resumeManager,
             coldResumeHandler)
      .get();
}

} // namespace client_server
} // namespace tests
} // namespace rsocket
