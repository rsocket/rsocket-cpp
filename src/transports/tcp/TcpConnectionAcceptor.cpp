// Copyright 2004-present Facebook. All Rights Reserved.

#include "TcpConnectionAcceptor.h"

#include <folly/ThreadName.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include "src/framing/FramedDuplexConnection.h"
#include "src/transports/tcp/TcpDuplexConnection.h"

using namespace reactivesocket;

namespace rsocket {

class TcpConnectionAcceptor::SocketCallback
    : public folly::AsyncServerSocket::AcceptCallback {
 public:
  explicit SocketCallback(std::function<void(
                              std::unique_ptr<reactivesocket::DuplexConnection>,
                              folly::EventBase&)>& onAccept)
      : onAccept_{onAccept} {}

  void connectionAccepted(
      int fd,
      const folly::SocketAddress&) noexcept override {
    LOG(INFO) << "Accepting TCP connection on FD " << fd;

    folly::AsyncSocket::UniquePtr socket(
        new folly::AsyncSocket(eventBase(), fd));

    auto connection = std::make_unique<TcpDuplexConnection>(
        std::move(socket), inlineExecutor());
    auto framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), inlineExecutor());

    onAccept_(std::move(framedConnection), *eventBase());
  }

  void acceptError(const std::exception& ex) noexcept override {
    LOG(INFO) << "TCP error: " << ex.what();
  }

  folly::EventBase* eventBase() const {
    return thread_.getEventBase();
  }

 private:
  /// The thread running this callback.
  folly::ScopedEventBaseThread thread_;

  /// Reference to the ConnectionAcceptor's callback.
  std::function<void(
      std::unique_ptr<reactivesocket::DuplexConnection>,
      folly::EventBase&)>& onAccept_;
};

////////////////////////////////////////////////////////////////////////////////

TcpConnectionAcceptor::TcpConnectionAcceptor(Options options)
    : options_(std::move(options)) {}

TcpConnectionAcceptor::~TcpConnectionAcceptor() {
  if (serverThread_) {
    stop();
  }
}

////////////////////////////////////////////////////////////////////////////////

folly::Future<folly::Unit> TcpConnectionAcceptor::start(
    std::function<void(std::unique_ptr<DuplexConnection>, folly::EventBase&)>
        acceptor) {
  if (onAccept_ != nullptr) {
    return folly::makeFuture<folly::Unit>(
        std::runtime_error("TcpConnectionAcceptor::start() already called"));
  }

  onAccept_ = std::move(acceptor);
  serverThread_ = std::make_unique<folly::ScopedEventBaseThread>();
  serverThread_->getEventBase()->runInEventBaseThread(
      [] { folly::setThreadName("TcpConnectionAcceptor.Listener"); });

  callbacks_.reserve(options_.threads);
  for (size_t i = 0; i < options_.threads; ++i) {
    callbacks_.push_back(std::make_unique<SocketCallback>(onAccept_));
    callbacks_[i]->eventBase()->runInEventBaseThread(
        [] { folly::setThreadName("TcpConnectionAcceptor.Worker"); });
  }

  LOG(INFO) << "Starting TCP listener on port " << options_.port << " with "
            << options_.threads << " request threads";

  serverSocket_.reset(
      new folly::AsyncServerSocket(serverThread_->getEventBase()));

  serverThread_->getEventBase()->runInEventBaseThread([this] {
    folly::SocketAddress addr;
    addr.setFromLocalPort(options_.port);

    serverSocket_->bind(addr);

    for (auto const& callback : callbacks_) {
      serverSocket_->addAcceptCallback(callback.get(), callback->eventBase());
    }

    serverSocket_->listen(options_.backlog);
    serverSocket_->startAccepting();

    for (auto& i : serverSocket_->getAddresses()) {
      LOG(INFO) << "Listening on " << i.describe();
    }
  });

  LOG(INFO) << "ConnectionAcceptor => leave start";
  return folly::unit;
}

void TcpConnectionAcceptor::stop() {
  LOG(INFO) << "Shutting down TCP listener";

  serverThread_->getEventBase()->runInEventBaseThread(
      [this] { serverSocket_.reset(); });
  serverThread_.reset();
}
}
