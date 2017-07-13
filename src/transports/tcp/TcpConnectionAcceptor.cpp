// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/transports/tcp/TcpConnectionAcceptor.h"

#include <folly/ThreadName.h>
#include <folly/futures/Future.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include "src/framing/FramedDuplexConnection.h"
#include "src/transports/tcp/TcpDuplexConnection.h"

namespace rsocket {

class TcpConnectionAcceptor::SocketCallback
    : public folly::AsyncServerSocket::AcceptCallback {
 public:
  explicit SocketCallback(OnDuplexConnectionAccept& onAccept)
      : onAccept_{onAccept} {}

  void connectionAccepted(
      int fd,
      const folly::SocketAddress& address) noexcept override {
    VLOG(1) << "Accepting TCP connection from " << address << " on FD " << fd;

    folly::AsyncSocket::UniquePtr socket(
        new folly::AsyncSocket(eventBase(), fd));

    auto connection = std::make_unique<TcpDuplexConnection>(
        std::move(socket));
    onAccept_(std::move(connection), *eventBase());
  }

  void acceptError(const std::exception& ex) noexcept override {
    VLOG(1) << "TCP error: " << ex.what();
  }

  folly::EventBase* eventBase() const {
    return thread_.getEventBase();
  }

 private:
  /// The thread running this callback.
  folly::ScopedEventBaseThread thread_;

  /// Reference to the ConnectionAcceptor's callback.
  OnDuplexConnectionAccept& onAccept_;
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

void TcpConnectionAcceptor::start(OnDuplexConnectionAccept onAccept) {
  if (onAccept_ != nullptr) {
    throw std::runtime_error("TcpConnectionAcceptor::start() already called");
  }

  onAccept_ = std::move(onAccept);
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

  // The AsyncServerSocket needs to be accessed from the listener thread only.
  // This will propagate out any exceptions the listener throws.
  folly::via(
      serverThread_->getEventBase(),
      [this] {
        folly::SocketAddress addr;
        addr.setFromLocalPort(options_.port);

        serverSocket_->bind(addr);

        for (auto const& callback : callbacks_) {
          serverSocket_->addAcceptCallback(
              callback.get(), callback->eventBase());
        }

        serverSocket_->listen(options_.backlog);
        serverSocket_->startAccepting();

        for (auto& i : serverSocket_->getAddresses()) {
          LOG(INFO) << "Listening on " << i.describe();
        }
      })
      .get();
}

void TcpConnectionAcceptor::stop() {
  LOG(INFO) << "Shutting down TCP listener";

  serverThread_->getEventBase()->runInEventBaseThread(
      [this] { serverSocket_.reset(); });
  serverThread_.reset();
}

folly::Optional<uint16_t> TcpConnectionAcceptor::listeningPort() const {
  if (!serverSocket_) {
    return folly::none;
  }
  return serverSocket_->getAddress().getPort();
}

}
