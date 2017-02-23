#include "rsocket/RSocketClient.h"
#include "src/StandardReactiveSocket.h"
#include "src/NullRequestHandler.h"
#include "src/folly/FollyKeepaliveTimer.h"

using namespace ::reactivesocket;

namespace rsocket {

RSocketClient::RSocketClient(
    std::unique_ptr<ConnectionFactory> connection)
    : lazyConnection(std::move(connection)) {
  LOG(INFO) << "RSocketClient => created";
}

Future<std::shared_ptr<StandardReactiveSocket>> RSocketClient::connect(
    ScopedEventBaseThread& eventBaseThread) {
  LOG(INFO) << "RSocketClient => start connection with Future";

  auto promise =
      std::make_shared<Promise<std::shared_ptr<StandardReactiveSocket>>>();

  lazyConnection->connect(
      [this, promise, &eventBaseThread](
          std::unique_ptr<DuplexConnection> framedConnection) {
        LOG(INFO) << "RSocketClient => onConnect received DuplexConnection";

        rs = StandardReactiveSocket::fromClientConnection(
            *eventBaseThread.getEventBase(),
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
                *eventBaseThread.getEventBase(),
                std::chrono::milliseconds(5000)));

        promise->setValue(rs);
      },
      eventBaseThread);

  return promise->getFuture();
}

RSocketClient::~RSocketClient() {
  LOG(INFO) << "RSocketClient => destroy";
}
}
