// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/transports/tcp/TcpConnectionFactory.h"

#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <glog/logging.h>
#include <folly/futures/Future.h>

#include "rsocket/transports/tcp/TcpDuplexConnection.h"

using namespace rsocket;

namespace rsocket {

namespace {

class ConnectCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  ConnectCallback(
      folly::SocketAddress address,
      folly::Promise<ConnectionResult> connectionResult)
      : address_(address), connectionResult_(std::move(connectionResult)) {
    VLOG(2) << "Constructing ConnectCallback";

    // Set up by ScopedEventBaseThread.
    auto evb = folly::EventBaseManager::get()->getExistingEventBase();
    DCHECK(evb);

    VLOG(3) << "Starting socket";
    socket_.reset(new folly::AsyncSocket(evb));

    VLOG(3) << "Attempting connection to " << address_;

    socket_->connect(this, address_);
  }

  ~ConnectCallback() {
    VLOG(2) << "Destroying ConnectCallback";
  }

  void connectSuccess() noexcept override {
    std::unique_ptr<ConnectCallback> deleter(this);

    VLOG(4) << "connectSuccess() on " << address_;

    auto connection = TcpConnectionFactory::createDuplexConnectionFromSocket(
        std::move(socket_), RSocketStats::noop());
    auto evb = folly::EventBaseManager::get()->getExistingEventBase();
    CHECK(evb);
    connectionResult_.setValue(ConnectionResult{std::move(connection), *evb});
  }

  void connectErr(const folly::AsyncSocketException &ex) noexcept override {
    std::unique_ptr<ConnectCallback> deleter(this);

    VLOG(4) << "connectErr(" << ex.what() << ") on " << address_;
    connectionResult_.setException(ex);
  }

 private:
  folly::SocketAddress address_;
  folly::AsyncSocket::UniquePtr socket_;
  folly::Promise<ConnectionResult> connectionResult_;
};

} // namespace

TcpConnectionFactory::TcpConnectionFactory(folly::SocketAddress address)
    : address_{std::move(address)} {
  VLOG(1) << "Constructing TcpConnectionFactory";
}

TcpConnectionFactory::~TcpConnectionFactory() {
  VLOG(1) << "Destroying TcpConnectionFactory";
}

folly::Future<ConnectionResult> TcpConnectionFactory::connect() {
  folly::Promise<ConnectionResult> promise;
  auto future = promise.getFuture();
  worker_.getEventBase()->runInEventBaseThread(
      [this, promise = std::move(promise)]() mutable {
        new ConnectCallback(address_, std::move(promise));
      });
  return future;
}

std::unique_ptr<DuplexConnection>
TcpConnectionFactory::createDuplexConnectionFromSocket(
    folly::AsyncSocket::UniquePtr socket,
    std::shared_ptr<RSocketStats> stats) {
  return std::make_unique<TcpDuplexConnection>(std::move(socket), std::move(stats));
}

} // namespace rsocket
