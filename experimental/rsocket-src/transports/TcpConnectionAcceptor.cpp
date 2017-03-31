// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/transports/TcpConnectionAcceptor.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace reactivesocket;
using namespace folly;

namespace rsocket {

TcpConnectionAcceptor::TcpConnectionAcceptor(int port) {
  addr_.setFromLocalPort(port);
}

std::unique_ptr<ConnectionAcceptor> TcpConnectionAcceptor::create(int port) {
  return std::make_unique<TcpConnectionAcceptor>(port);
}

void TcpConnectionAcceptor::start(
    std::function<void(std::unique_ptr<DuplexConnection>, EventBase&)>
        acceptor) {
  // TODO needs to blow up if called more than once
  LOG(INFO) << "ConnectionAcceptor => start";
  onAccept_ = std::move(acceptor);
  // TODO need to support more than 1 thread
  serverSocket_ = AsyncServerSocket::newSocket(&eventBase_);
  auto thread = std::thread([this]() { eventBase_.loopForever(); });
  thread.detach();
  eventBase_.runInEventBaseThread([this]() {
    LOG(INFO) << "ConnectionAcceptor => start in loop";
    serverSocket_->bind(addr_);
    serverSocket_->addAcceptCallback(this, &eventBase_);
    serverSocket_->listen(10);
    serverSocket_->startAccepting();

    for (auto i : serverSocket_->getAddresses()) {
      LOG(INFO) << "ConnectionAcceptor => listening on => " << i.describe();
    }
  });

  LOG(INFO) << "ConnectionAcceptor => leave start";
}

void TcpConnectionAcceptor::connectionAccepted(
    int fd,
    const SocketAddress& clientAddr) noexcept {
  LOG(INFO) << "ConnectionAcceptor => accept connection " << fd;
  auto socket = folly::AsyncSocket::UniquePtr(new AsyncSocket(&eventBase_, fd));

  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), inlineExecutor());

  onAccept_(std::move(framedConnection), eventBase_);
}

void TcpConnectionAcceptor::acceptError(const std::exception& ex) noexcept {
  LOG(INFO) << "ConnectionAcceptor => error => " << ex.what();
}

TcpConnectionAcceptor::~TcpConnectionAcceptor() {
  LOG(INFO) << "ConnectionAcceptor => destroy";
}
}
