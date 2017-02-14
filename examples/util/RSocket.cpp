// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocket.h"
#include <glog/logging.h>
#include <src/NullRequestHandler.h>
#include "src/folly/FollyKeepaliveTimer.h"

namespace rsocket {

std::unique_ptr<RSocket> RSocket::createClientFactory(
    std::unique_ptr<ConnectionFactory> connection) {
  return std::make_unique<RSocket>(std::move(connection));
}

RSocket::RSocket(std::unique_ptr<ConnectionFactory> connection)
    : lazyConnection(std::move(connection)) {
  LOG(INFO) << "RSocket => created";
}

Future<std::shared_ptr<StandardReactiveSocket>> RSocket::connect(
    ScopedEventBaseThread& eventBaseThread) {
  LOG(INFO) << "RSocket => start connection with Future";

  auto promise =
      std::make_shared<Promise<std::shared_ptr<StandardReactiveSocket>>>();

  lazyConnection->connect(
      [this, promise, &eventBaseThread](
          std::unique_ptr<DuplexConnection> framedConnection) {
        LOG(INFO) << "RSocket => onConnect received DuplexConnection";

        rs = StandardReactiveSocket::fromClientConnection(
            *eventBaseThread.getEventBase(),
            std::move(framedConnection),
            // TODO need to optionally allow this being passed in for a duplex client
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

RSocket::~RSocket() {
  LOG(INFO) << "RSocket => destroy";
}
}