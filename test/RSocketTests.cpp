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
  opts.threads = 10;
  opts.backlog = 200;
  opts.address = folly::SocketAddress("::", 0);
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  rs->start(std::move(serviceHandler));
  return rs;
}

folly::Future<std::unique_ptr<RSocketClient>> makeClientAsync(
    folly::EventBase* eventBase,
    uint16_t port) {
  CHECK(eventBase);
  return RSocket::createConnectedClient(getConnFactory(eventBase, port));
}

std::unique_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    uint16_t port) {
  return makeClientAsync(eventBase, port).get();
}

namespace {
struct DisconnectedResponder : public rsocket::RSocketResponder {
  DisconnectedResponder() {}

  yarpl::Reference<yarpl::single::Single<rsocket::Payload>>
  handleRequestResponse(rsocket::Payload, rsocket::StreamId) override {
    CHECK(false);
    return nullptr;
  }

  yarpl::Reference<yarpl::flowable::Flowable<rsocket::Payload>>
  handleRequestStream(rsocket::Payload, rsocket::StreamId) override {
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
}

std::unique_ptr<RSocketClient> makeDisconnectedClient(
    folly::EventBase* eventBase) {
  auto server =
      makeServer(std::make_shared<DisconnectedResponder>());

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
