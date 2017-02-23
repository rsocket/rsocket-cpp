// Copyright 2004-present Facebook. All Rights Reserved.

#include <rsocket/transports/TcpConnectionAcceptor.h>
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

std::unique_ptr<ConnectionAcceptor> TcpConnectionAcceptor::create(
    int port) {
  return std::make_unique<TcpConnectionAcceptor>(port);
}

void TcpConnectionAcceptor::start(std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)> acceptor) {
  // TODO needs to blow up if called more than once
  LOG(INFO) << "ConnectionAcceptor => start";
  onAccept = std::move(acceptor);
  // TODO need to support more than 1 thread
  serverSocket = AsyncServerSocket::newSocket(&eventBase);
  thread = std::thread([this]() { eventBase.loopForever(); });
  thread.detach();
  eventBase.runInEventBaseThread([this]() {
    LOG(INFO) << "ConnectionAcceptor => start in loop";
    serverSocket->setReusePortEnabled(true);
    serverSocket->bind(addr);
    serverSocket->addAcceptCallback(this, &eventBase);
    serverSocket->listen(10);
    serverSocket->startAccepting();

    for (auto i : serverSocket->getAddresses()) {
      LOG(INFO) << "ConnectionAcceptor => listening on => "
                << i.describe();
    }
  });

  LOG(INFO) << "ConnectionAcceptor => leave start";
}

void TcpConnectionAcceptor::connectionAccepted(
    int fd,
    const SocketAddress& clientAddr) noexcept {
  LOG(INFO) << "ConnectionAcceptor => accept connection " << fd;
  auto socket = folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase, fd));

  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), inlineExecutor());

  onAccept(std::move(framedConnection), eventBase);
}

void TcpConnectionAcceptor::acceptError(
    const std::exception& ex) noexcept {
  LOG(INFO) << "ConnectionAcceptor => error => " << ex.what();
}

TcpConnectionAcceptor::~TcpConnectionAcceptor() {
  LOG(INFO) << "ConnectionAcceptor => destroy";
}
}