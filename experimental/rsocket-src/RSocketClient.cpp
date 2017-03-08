// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketClient.h"
#include "rsocket/RSocketRequester.h"
#include "src/NullRequestHandler.h"
#include "src/StandardReactiveSocket.h"
#include "src/folly/FollyKeepaliveTimer.h"

using namespace reactivesocket;
using namespace folly;

namespace rsocket {

RSocketClient::RSocketClient(std::unique_ptr<ConnectionFactory> connection)
    : lazyConnection_(std::move(connection)) {
  LOG(INFO) << "RSocketClient => created";
}

Future<std::shared_ptr<RSocketRequester>> RSocketClient::connect() {
  LOG(INFO) << "RSocketClient => start connection with Future";

  auto promise = std::make_shared<Promise<std::shared_ptr<RSocketRequester>>>();

  lazyConnection_->connect([this, promise](
      std::unique_ptr<DuplexConnection> framedConnection,
      EventBase& eventBase) {
    LOG(INFO) << "RSocketClient => onConnect received DuplexConnection";

    auto r = StandardReactiveSocket::fromClientConnection(
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

    auto rsocket = RSocketRequester::create(std::move(r), eventBase);
    // store it so it lives as long as the RSocketClient
    rsockets_.push_back(rsocket);
    promise->setValue(rsocket);
  });

  return promise->getFuture();
}

RSocketClient::~RSocketClient() {
  LOG(INFO) << "RSocketClient => destroy";
}
}
