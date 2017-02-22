// Copyright 2004-present Facebook. All Rights Reserved.

#include "experimental/rsocket/RSocket.h"
#include <src/NullRequestHandler.h>
#include "src/folly/FollyKeepaliveTimer.h"

namespace rsocket {

std::unique_ptr<RSocketClient> RSocket::createClientFactory(
    std::unique_ptr<ClientConnectionFactory> connectionFactory) {
  return std::make_unique<RSocketClient>(std::move(connectionFactory));
}

std::unique_ptr<RSocketServer> RSocket::createServer(
    std::unique_ptr<ServerConnectionAcceptor> connectionAcceptor,
    HandlerFactory handlerFactory) {
  return std::make_unique<RSocketServer>(
      std::move(connectionAcceptor), std::move(handlerFactory));
}

RSocketClient::RSocketClient(
    std::unique_ptr<ClientConnectionFactory> connection)
    : lazyConnection(std::move(connection)) {
  LOG(INFO) << "RSocketClient => created";
}

Future<std::shared_ptr<StandardReactiveSocket>> RSocketClient::connect(
    ScopedEventBaseThread& eventBaseThread) {
  LOG(INFO) << "RSocketClient => start connection with Future";

  auto promise =
      std::make_shared<Promise<std::shared_ptr<StandardReactiveSocket>>>();

  lazyConnection->connect(
      [this, promise, &eventBaseThread](
          std::unique_ptr<DuplexConnection> framedConnection) {
        LOG(INFO) << "RSocketClient => onConnect received DuplexConnection";

        rs = StandardReactiveSocket::fromClientConnection(
            *eventBaseThread.getEventBase(),
            std::move(framedConnection),
            // TODO need to optionally allow this being passed in for a duplex
            // client
            std::make_unique<NullRequestHandler>(),
            // TODO need to allow this being passed in
            ConnectionSetupPayload(
                "text/plain", "text/plain", Payload("meta", "data")),
            Stats::noop(),
            // TODO need to optionally allow defining the keepalive timer
            std::make_unique<FollyKeepaliveTimer>(
                *eventBaseThread.getEventBase(),
                std::chrono::milliseconds(5000)));

        promise->setValue(rs);
      },
      eventBaseThread);

  return promise->getFuture();
}

RSocketClient::~RSocketClient() {
  LOG(INFO) << "RSocketClient => destroy";
}

RSocketServer::RSocketServer(
    std::unique_ptr<ServerConnectionAcceptor> connectionAcceptor,
    HandlerFactory hf)
    : lazyAcceptor(std::move(connectionAcceptor)), handlerFactory(hf) {}

void RSocketServer::start() {
  lazyAcceptor->start([this](
      std::unique_ptr<DuplexConnection> duplexConnection,
      EventBase& eventBase) {
    LOG(INFO) << "RSocketClient => received new connection";

    auto rs = StandardReactiveSocket::fromServerConnection(
        eventBase, std::move(duplexConnection), handlerFactory());

    // register callback for removal when the socket closes
    rs->onClosed([ this, rs = rs.get() ](const folly::exception_wrapper& ex) {
      removeSocket(*rs);
      LOG(INFO) << "RSocketClient => removed closed connection";
    });

    // store this ReactiveSocket
    reactiveSockets_.push_back(std::move(rs));
  });
}

void RSocketServer::shutdown() {
  shuttingDown = true;
  reactiveSockets_.clear();
}

void RSocketServer::removeSocket(ReactiveSocket& socket) {
  if (!shuttingDown) {
    reactiveSockets_.erase(std::remove_if(
        reactiveSockets_.begin(),
        reactiveSockets_.end(),
        [&socket](std::unique_ptr<StandardReactiveSocket>& vecSocket) {
          return vecSocket.get() == &socket;
        }));
  }
}
}