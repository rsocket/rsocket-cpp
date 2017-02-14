// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <rsocket/transports/TcpConnectionFactory.h>
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

class TcpConnectionInstance : public AsyncSocket::ConnectCallback {
 public:
  TcpConnectionInstance(OnConnect onConnect, SocketAddress& addr)
      : addr_(addr), onConnect_(onConnect) {}

  void connect() {
    // now start the connection asynchronously
    eventBase_->runInEventBaseThreadAndWait([this]() {
      LOG(INFO) << "ConnectionFactory => starting socket";
      socket_.reset(new folly::AsyncSocket(eventBase_));

      LOG(INFO) << "ConnectionFactory => attempting connection to "
                << addr_.describe() << std::endl;

      socket_->connect(this, addr_);

      LOG(INFO) << "ConnectionFactory  => DONE connect";
    });
  }

 private:
  SocketAddress& addr_;
  folly::AsyncSocket::UniquePtr socket_;
  OnConnect onConnect_;
  ScopedEventBaseThread eventBaseThread_;
  EventBase* eventBase_{eventBaseThread_.getEventBase()};

  void connectSuccess() noexcept {
    LOG(INFO) << "ConnectionFactory => socketCallback => Success";

    std::unique_ptr<DuplexConnection> connection =
        std::make_unique<TcpDuplexConnection>(
            std::move(socket_), inlineExecutor(), Stats::noop());
    std::unique_ptr<DuplexConnection> framedConnection =
        std::make_unique<FramedDuplexConnection>(
            std::move(connection), inlineExecutor());

    // callback with the connection now that we have it
    onConnect_(std::move(framedConnection), *eventBase_);
  }

  void connectErr(const AsyncSocketException& ex) noexcept {
    LOG(INFO) << "ConnectionFactory => socketCallback => ERROR => " << ex.what()
              << " " << ex.getType() << std::endl;
  }
};

TcpConnectionFactory::TcpConnectionFactory(std::string host, int port)
    : addr(host, port, true) {}

void TcpConnectionFactory::connect(OnConnect oc) {
  auto c = std::make_unique<TcpConnectionInstance>(std::move(oc), addr);
  c->connect();
  connections_.push_back(std::move(c));
}

std::unique_ptr<ConnectionFactory> TcpConnectionFactory::create(
    std::string host,
    int port) {
  LOG(INFO) << "ConnectionFactory creation => host: " << host
            << " port: " << port;
  return std::make_unique<TcpConnectionFactory>(host, port);
}

TcpConnectionFactory::~TcpConnectionFactory() {
  LOG(INFO) << "ConnectionFactory => destroy";
}
}