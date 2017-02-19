
#include "TcpServerConnectionAcceptor.h"
#include <iostream>
#include "src/StandardReactiveSocket.h"
#include "src/SubscriptionBase.h"
#include "src/folly/FollyKeepaliveTimer.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

std::unique_ptr<ServerConnectionAcceptor> ServerConnectionAcceptor::tcpServer(
    int port) {
  return std::make_unique<ServerConnectionAcceptor>(port);
}

void ServerConnectionAcceptor::start(OnAccept acceptor) {
  // TODO needs to blow up if called more than once
  LOG(INFO) << "ServerConnectionAcceptor => start";
  onAccept = std::move(acceptor);
  // TODO need to support more than 1 thread
  serverSocket = AsyncServerSocket::newSocket(&eventBase);
  thread = std::thread([this]() { eventBase.loopForever(); });
  thread.detach();
  eventBase.runInEventBaseThread([this]() {
    LOG(INFO) << "ServerConnectionAcceptor => start in loop";
    serverSocket->setReusePortEnabled(true);
    serverSocket->bind(addr);
    serverSocket->addAcceptCallback(this, &eventBase);
    serverSocket->listen(10);
    serverSocket->startAccepting();

    for (auto i : serverSocket->getAddresses()) {
      LOG(INFO) << "ServerConnectionAcceptor => listening on => "
                << i.describe();
    }
  });

  LOG(INFO) << "ServerConnectionAcceptor => leave start";
}

void ServerConnectionAcceptor::connectionAccepted(
    int fd,
    const SocketAddress& clientAddr) noexcept {
  LOG(INFO) << "ServerConnectionAcceptor => accept connection " << fd;
  auto socket = folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase, fd));

  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), inlineExecutor());

  onAccept(std::move(framedConnection), eventBase);
}

void ServerConnectionAcceptor::acceptError(const std::exception& ex) noexcept {
  LOG(INFO) << "ServerConnectionAcceptor => error => " << ex.what();
}

ServerConnectionAcceptor::~ServerConnectionAcceptor() {
  LOG(INFO) << "ServerConnectionAcceptor => destroy";
}
}