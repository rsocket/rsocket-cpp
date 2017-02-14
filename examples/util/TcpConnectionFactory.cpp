
#include "TcpConnectionFactory.h"

#include <iostream>
#include "src/StandardReactiveSocket.h"
#include "src/SubscriptionBase.h"
#include "src/folly/FollyKeepaliveTimer.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

void ConnectionFactory::connect(OnConnect oc, ScopedEventBaseThread& eventBaseThread) {
  // TODO not happy with this being copied here
  onConnect = oc;
  // now start the connection asynchronously
  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait([this, &eventBaseThread]() {
    LOG(INFO) << "ConnectionFactory => starting socket";
    socket.reset(new folly::AsyncSocket(eventBaseThread.getEventBase()));

    LOG(INFO) << "ConnectionFactory => attempting connection to "
              << addr.describe() << std::endl;

    socket->connect(this, addr);

    LOG(INFO) << "ConnectionFactory  => DONE connect";
  });
}

void ConnectionFactory::connectSuccess() noexcept {
  LOG(INFO) << "ConnectionFactory => socketCallback => Success";

  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor(), Stats::noop());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), inlineExecutor());

  // callback with the connection now that we have it
  onConnect(std::move(framedConnection));
}

void ConnectionFactory::connectErr(const AsyncSocketException& ex) noexcept {
  LOG(INFO) << "ConnectionFactory => socketCallback => ERROR => " << ex.what()
            << " " << ex.getType() << std::endl;
}

std::unique_ptr<ConnectionFactory> ConnectionFactory::tcpClient(
    std::string host,
    int port) {
  LOG(INFO) << "ConnectionFactory creation => host: " << host << " port: " << port;
  return std::make_unique<ConnectionFactory>(host, port);
}

ConnectionFactory::~ConnectionFactory() {
  LOG(INFO) << "ConnectionFactory => destroy";
}
}