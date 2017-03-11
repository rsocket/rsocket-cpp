// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/transports/TcpConnectionFactory.h"
#include <folly/io/async/AsyncSocket.h>
#include <folly/io/async/EventBaseManager.h>
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace reactivesocket;
using namespace folly;

namespace rsocket {

// create new ScopedEventBaseThread
// create new EventBase from it
// create new AsyncSocket
// connect
// create FramedDuplexConnection
// TODO create variant that takes an existing EventBase
class SocketConnectorAndCallback : public AsyncSocket::ConnectCallback {
 public:
  SocketConnectorAndCallback(
      OnConnect onConnect,
      SocketAddress addr,
      EventBase* eventBase)
      : addr_(addr), onConnect_(onConnect), eventBase_(eventBase) {}

  ~SocketConnectorAndCallback() {
    LOG(INFO) << "SocketConnectorAndCallback => destroy";
  }

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
  SocketAddress addr_;
  folly::AsyncSocket::UniquePtr socket_;
  OnConnect onConnect_;
  EventBase* eventBase_;

  void connectSuccess() noexcept {
    LOG(INFO) << "ConnectionFactory => socketCallback => Success";

    // safe way to call 'delete this'
    auto uThis = std::unique_ptr<SocketConnectorAndCallback>(this);

    auto connection = std::make_unique<TcpDuplexConnection>(
        std::move(socket_), inlineExecutor(), Stats::noop());
    auto framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), inlineExecutor());

    // callback with the connection now that we have it
    onConnect_(std::move(framedConnection), *eventBase_);
  }

  void connectErr(const AsyncSocketException& ex) noexcept {
    LOG(INFO) << "ConnectionFactory => socketCallback => ERROR => " << ex.what()
              << " " << ex.getType() << std::endl;

    delete this;
  }
};

TcpConnectionFactory::TcpConnectionFactory(std::string host, uint16_t port)
    : addr_(host, port, true),
      eventBaseThread_(std::make_unique<ScopedEventBaseThread>()) {}

void TcpConnectionFactory::connect(OnConnect oc) {
  // uses 'new' here since it needs to live while doing work asynchronously
  // it is deleted in the connectSuccess/connectErr methods in the class
  auto c = new SocketConnectorAndCallback(
      std::move(oc), addr_, eventBaseThread_->getEventBase());
  c->connect();
}

std::unique_ptr<ConnectionFactory> TcpConnectionFactory::create(
    std::string host,
    uint16_t port) {
  LOG(INFO) << "ConnectionFactory creation => host: " << host
            << " port: " << port;
  return std::make_unique<TcpConnectionFactory>(host, port);
}

TcpConnectionFactory::~TcpConnectionFactory() {
  LOG(INFO) << "ConnectionFactory => destroy";
}
}
