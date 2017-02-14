#include "rsocket/RSocketClient.h"
#include "src/NullRequestHandler.h"
#include "src/StandardReactiveSocket.h"
#include "src/folly/FollyKeepaliveTimer.h"

using namespace ::reactivesocket;

namespace rsocket {

RSocketClient::RSocketClient(std::unique_ptr<ConnectionFactory> connection)
    : lazyConnection(std::move(connection)) {
  LOG(INFO) << "RSocketClient => created";
}
// TODO make unique_ptr
Future<std::shared_ptr<StandardReactiveSocket>> RSocketClient::connect() {
  LOG(INFO) << "RSocketClient => start connection with Future";

  auto promise =
      std::make_shared<Promise<std::shared_ptr<StandardReactiveSocket>>>();

  lazyConnection->connect([this, promise](
      std::unique_ptr<DuplexConnection> framedConnection,
      EventBase& eventBase) {
    LOG(INFO) << "RSocketClient => onConnect received DuplexConnection";

    rs = StandardReactiveSocket::fromClientConnection(
        eventBase,
        std::move(framedConnection),
        // TODO need to optionally allow this being passed in for a duplex
        // client
        std::make_unique<NullRequestHandler>(),
        // TODO need to allow this being passed in
        ConnectionSetupPayload(
            "text/plain", "text/plain", Payload("meta", "data")),
        Stats::noop(),
        // TODO need to optionally allow defining the keepalive timer
        std::make_unique<FollyKeepaliveTimer>(
            eventBase, std::chrono::milliseconds(5000)));

    promise->setValue(rs);
  });

  return promise->getFuture();
}

RSocketClient::~RSocketClient() {
  LOG(INFO) << "RSocketClient => destroy";
}
}
