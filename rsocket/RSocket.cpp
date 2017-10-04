// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocket.h"

namespace rsocket {

folly::Future<std::unique_ptr<RSocketClient>> RSocket::createConnectedClient(
    std::shared_ptr<ConnectionFactory> connectionFactory,
    SetupParameters setupParameters,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    folly::EventBase* stateMachineEvb) {
  auto createRSC = [
    connectionFactory,
    setupParameters = std::move(setupParameters),
    responder = std::move(responder),
    keepaliveTimer = std::move(keepaliveTimer),
    stats = std::move(stats),
    connectionEvents = std::move(connectionEvents),
    resumeManager = std::move(resumeManager),
    coldResumeHandler = std::move(coldResumeHandler),
    stateMachineEvb
  ](ConnectionFactory::ConnectedDuplexConnection connection) mutable {
    VLOG(3) << "createConnectedClient received DuplexConnection";
    return RSocket::createClientFromConnection(
        std::move(connection.connection),
        connection.eventBase,
        std::move(setupParameters),
        std::move(connectionFactory),
        std::move(responder),
        std::move(keepaliveTimer),
        std::move(stats),
        std::move(connectionEvents),
        std::move(resumeManager),
        std::move(coldResumeHandler),
        stateMachineEvb);
  };

  return connectionFactory->connect().then([createRSC = std::move(createRSC)](
      ConnectionFactory::ConnectedDuplexConnection connection) mutable {
    // fromConnection method must be called from the transport eventBase
    // and since there is no guarantee that the Future returned from the
    // connectionFactory::connect method is executed on the event base, we
    // have to ensure it by using folly::via
    auto* transportEvb = &connection.eventBase;
    return via(transportEvb, [
      connection = std::move(connection),
      createRSC = std::move(createRSC)
    ]() mutable { return createRSC(std::move(connection)); });
  });
}

folly::Future<std::unique_ptr<RSocketClient>> RSocket::createResumedClient(
    std::shared_ptr<ConnectionFactory> connectionFactory,
    ResumeIdentificationToken token,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    ProtocolVersion protocolVersion,
    folly::EventBase* stateMachineEvb) {
  auto* c = new RSocketClient(
      std::move(connectionFactory),
      std::move(protocolVersion),
      std::move(token),
      std::move(responder),
      std::move(keepaliveTimer),
      std::move(stats),
      std::move(connectionEvents),
      std::move(resumeManager),
      std::move(coldResumeHandler),
      stateMachineEvb);

  return c->resume()
      .then([client = std::unique_ptr<RSocketClient>(c)]() mutable {
        return std::move(client);
      });
}

std::unique_ptr<RSocketClient> RSocket::createClientFromConnection(
    std::unique_ptr<DuplexConnection> connection,
    folly::EventBase& transportEvb,
    SetupParameters setupParameters,
    std::shared_ptr<ConnectionFactory> connectionFactory,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    folly::EventBase* stateMachineEvb) {
  auto c = std::unique_ptr<RSocketClient>(new RSocketClient(
      std::move(connectionFactory),
      setupParameters.protocolVersion,
      setupParameters.token,
      std::move(responder),
      std::move(keepaliveTimer),
      std::move(stats),
      std::move(connectionEvents),
      std::move(resumeManager),
      std::move(coldResumeHandler),
      stateMachineEvb));
  c->fromConnection(
      std::move(connection),
      transportEvb,
      std::move(setupParameters));
  return c;
}

std::unique_ptr<RSocketServer> RSocket::createServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor,
    std::shared_ptr<RSocketStats> stats) {
  return std::make_unique<RSocketServer>(
      std::move(connectionAcceptor), std::move(stats));
}
} // namespace rsocket
