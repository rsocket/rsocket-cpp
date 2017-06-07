// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RSocketClient.h"
#include "src/RSocketRequester.h"
#include "src/RSocketResponder.h"
#include "src/RSocketStats.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/internal/RSocketConnectionManager.h"
#include "src/framing/FrameTransport.h"

using namespace folly;

namespace rsocket {

RSocketClient::RSocketClient(
    std::unique_ptr<ConnectionFactory> connectionFactory)
    : connectionFactory_(std::move(connectionFactory)),
      connectionManager_(std::make_unique<RSocketConnectionManager>()) {
  VLOG(1) << "Constructing RSocketClient";
}

RSocketClient::~RSocketClient() {
  VLOG(1) << "Destroying RSocketClient";
}

folly::Future<std::unique_ptr<RSocketRequester>> RSocketClient::connect(
    SetupParameters setupParameters,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats) {
  VLOG(2) << "Starting connection";

  folly::Promise<std::unique_ptr<RSocketRequester>> promise;
  auto future = promise.getFuture();

  connectionFactory_->connect([
    this,
    setupParameters = std::move(setupParameters),
    responder = std::move(responder),
    keepaliveTimer = std::move(keepaliveTimer),
    stats = std::move(stats),
    networkStats = std::move(networkStats),
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
        std::move(keepaliveTimer),
        ReactiveSocketMode::CLIENT,
        std::move(stats),
        std::move(networkStats));

    connectionManager_->manageConnection(rs, eventBase);

    rs->connectClientSendSetup(std::move(connection), std::move(setupParameters));

    auto rsocket = std::make_unique<RSocketRequester>(std::move(rs), eventBase);
    promise.setValue(std::move(rsocket));
  });

  return future;
}

}
