// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketClient.h"
#include "RSocketRequester.h"
#include "RSocketResponder.h"
#include "RSocketStats.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/framing/FrameTransport.h"

using namespace folly;

namespace rsocket {

RSocketClient::RSocketClient(
    std::unique_ptr<ConnectionFactory> connectionFactory)
    : connectionFactory_(std::move(connectionFactory)) {
  VLOG(1) << "Constructing RSocketClient";
}

folly::Future<std::shared_ptr<RSocketRequester>> RSocketClient::connect(
    SetupParameters setupParameters,
    std::shared_ptr<RSocketResponder> responder,
    std::shared_ptr<RSocketStats> stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer) {
  VLOG(2) << "Starting connection";

  folly::Promise<std::shared_ptr<RSocketRequester>> promise;
  auto future = promise.getFuture();

  connectionFactory_->connect([
    this,
    setupParameters = std::move(setupParameters),
    responder = std::move(responder),
    stats = std::move(stats),
    keepaliveTimer = std::move(keepaliveTimer),
    promise = std::move(promise)](
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase& eventBase) mutable {
    VLOG(3) << "onConnect received DuplexConnection";

    if (!responder) {
      responder = std::make_shared<RSocketResponder>();
    }

    if (!stats) {
      stats = RSocketStats::noop();
    }

    if (!keepaliveTimer) {
      keepaliveTimer = std::make_unique<FollyKeepaliveTimer>(
          eventBase, std::chrono::milliseconds(5000));
    }

    auto rs = std::make_shared<RSocketStateMachine>(
        eventBase,
        std::move(responder),
        std::move(stats),
        std::move(keepaliveTimer),
        ReactiveSocketMode::CLIENT);
    rs->connectClientSendSetup(std::move(connection), std::move(setupParameters));

    auto rsocket = RSocketRequester::create(std::move(rs), eventBase);

    // store it so it lives as long as the RSocketClient
    rsockets_.push_back(rsocket);
    promise.setValue(std::move(rsocket));
  });

  return future;
}

RSocketClient::~RSocketClient() {
  VLOG(1) << "Destroying RSocketClient";
}
}
