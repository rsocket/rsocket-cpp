
#include <rsocket/transports/TcpClientConnectionFactory.h>
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

using namespace ::reactivesocket;
using namespace ::folly;

namespace rsocket {

void TcpClientConnectionFactory::connect(
    OnConnect oc,
    ScopedEventBaseThread& eventBaseThread) {
  // TODO not happy with this being copied here
  onConnect = oc;
  // now start the connection asynchronously
  eventBaseThread.getEventBase()->runInEventBaseThreadAndWait(
      [this, &eventBaseThread]() {
        LOG(INFO) << "ClientConnectionFactory => starting socket";
        socket.reset(new folly::AsyncSocket(eventBaseThread.getEventBase()));

        LOG(INFO) << "ClientConnectionFactory => attempting connection to "
                  << addr.describe() << std::endl;

        socket->connect(this, addr);

        LOG(INFO) << "ClientConnectionFactory  => DONE connect";
      });
}

void TcpClientConnectionFactory::connectSuccess() noexcept {
  LOG(INFO) << "ClientConnectionFactory => socketCallback => Success";

  std::unique_ptr<DuplexConnection> connection =
      std::make_unique<TcpDuplexConnection>(
          std::move(socket), inlineExecutor(), Stats::noop());
  std::unique_ptr<DuplexConnection> framedConnection =
      std::make_unique<FramedDuplexConnection>(
          std::move(connection), inlineExecutor());

  // callback with the connection now that we have it
  onConnect(std::move(framedConnection));
}

void TcpClientConnectionFactory::connectErr(
    const AsyncSocketException& ex) noexcept {
  LOG(INFO) << "ClientConnectionFactory => socketCallback => ERROR => "
            << ex.what() << " " << ex.getType() << std::endl;
}

std::unique_ptr<ClientConnectionFactory> TcpClientConnectionFactory::create(
        std::string host,
        int port) {
  LOG(INFO) << "ClientConnectionFactory creation => host: " << host
            << " port: " << port;
  return std::make_unique<TcpClientConnectionFactory>(host, port);
}

TcpClientConnectionFactory::~TcpClientConnectionFactory() {
  LOG(INFO) << "ClientConnectionFactory => destroy";
}
}