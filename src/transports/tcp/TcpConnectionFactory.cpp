// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/transports/tcp/TcpConnectionFactory.h"

#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <glog/logging.h>

#include "src/transports/tcp/TcpDuplexConnection.h"

using namespace rsocket;

namespace rsocket {

namespace {

class ConnectCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  ConnectCallback(
      folly::SocketAddress address,
      OnDuplexConnectionConnect onConnect)
      : address_(address), onConnect_{std::move(onConnect)} {
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
    onConnect_(std::move(connection), *evb);
  }

  void connectErr(const folly::AsyncSocketException& ex) noexcept override {
    std::unique_ptr<ConnectCallback> deleter(this);

    VLOG(4) << "connectErr(" << ex.what() << ") on " << address_;
  }

 private:
  folly::SocketAddress address_;
  folly::AsyncSocket::UniquePtr socket_;
  OnDuplexConnectionConnect onConnect_;
};

} // namespace

TcpConnectionFactory::TcpConnectionFactory(folly::SocketAddress address)
    : address_{std::move(address)} {
  VLOG(1) << "Constructing TcpConnectionFactory";
}

TcpConnectionFactory::~TcpConnectionFactory() {
  VLOG(1) << "Destroying TcpConnectionFactory";
}

void TcpConnectionFactory::connect(OnDuplexConnectionConnect cb) {
  worker_.getEventBase()->runInEventBaseThread(
      [ this, fn = std::move(cb) ]() mutable {
        new ConnectCallback(address_, std::move(fn));
      });
}

std::unique_ptr<DuplexConnection>
TcpConnectionFactory::createDuplexConnectionFromSocket(
    folly::AsyncSocket::UniquePtr socket,
    std::shared_ptr<RSocketStats> stats) {
  return std::make_unique<TcpDuplexConnection>(std::move(socket), std::move(stats));
}

} // namespace rsocket
