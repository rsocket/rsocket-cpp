// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/transports/TcpConnectionFactory.h"

#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <glog/logging.h>

#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace reactivesocket;

namespace rsocket {

namespace {

class ConnectCallback : public folly::AsyncSocket::ConnectCallback {
 public:
  ConnectCallback(folly::SocketAddress address, OnConnect onConnect)
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

  void connectSuccess() noexcept {
    std::unique_ptr<ConnectCallback> deleter(this);

    auto evb = folly::EventBaseManager::get()->getExistingEventBase();

    VLOG(4) << "connectSuccess() on " << address_;

    auto connection = std::make_unique<TcpDuplexConnection>(
        std::move(socket_), *evb, Stats::noop());
    auto framedConnection =
        std::make_unique<FramedDuplexConnection>(std::move(connection), *evb);

    onConnect_(std::move(framedConnection), *evb);
  }

  void connectErr(const folly::AsyncSocketException& ex) noexcept {
    std::unique_ptr<ConnectCallback> deleter(this);

    VLOG(4) << "connectErr(" << ex.what() << ") on " << address_;
  }

 private:
  folly::SocketAddress address_;
  folly::AsyncSocket::UniquePtr socket_;
  OnConnect onConnect_;
};

} // namespace

TcpConnectionFactory::TcpConnectionFactory(folly::SocketAddress address)
    : address_{std::move(address)} {
  VLOG(1) << "Constructing TcpConnectionFactory";
}

void TcpConnectionFactory::connect(OnConnect cb) {
  worker_.getEventBase()->runInEventBaseThread(
      [ this, fn = std::move(cb) ]() mutable {
        new ConnectCallback(address_, std::move(fn));
      });
}

TcpConnectionFactory::~TcpConnectionFactory() {
  VLOG(1) << "Destroying TcpConnectionFactory";
}
} // namespace rsocket
