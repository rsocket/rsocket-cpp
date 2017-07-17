// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketClient.h"
#include "rsocket/RSocketRequester.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/framing/FrameTransport.h"
#include "rsocket/framing/FramedDuplexConnection.h"
#include "rsocket/internal/FollyKeepaliveTimer.h"
#include "rsocket/internal/RSocketConnectionManager.h"

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

    auto rsocket = fromConnection(
        std::move(connection),
        eventBase,
        std::move(setupParameters),
        std::move(responder),
        std::move(keepaliveTimer),
        std::move(stats),
        std::move(networkStats));
    promise.setValue(std::move(rsocket));
  });

  return future;
}

std::unique_ptr<RSocketRequester> RSocketClient::fromConnection(
    std::unique_ptr<DuplexConnection> connection,
    folly::EventBase& eventBase,
    SetupParameters setupParameters,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketNetworkStats> networkStats) {
  CHECK(eventBase.isInEventBaseThread());

  if (!responder) {
    responder = std::make_shared<RSocketResponder>();
  }

  if (!keepaliveTimer) {
    keepaliveTimer = std::make_unique<FollyKeepaliveTimer>(
        eventBase, std::chrono::milliseconds(5000));
  }

  if (!stats) {
    stats = RSocketStats::noop();
  }

  auto rs = std::make_shared<RSocketStateMachine>(
      eventBase,
      std::move(responder),
      std::move(keepaliveTimer),
      ReactiveSocketMode::CLIENT,
      std::move(stats),
      std::move(networkStats));

  connectionManager_->manageConnection(rs, eventBase);

  std::unique_ptr<DuplexConnection> framedConnection;
  if (connection->isFramed()) {
    framedConnection = std::move(connection);
  } else {
    framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), setupParameters.protocolVersion);
  }

  rs->connectClientSendSetup(std::move(framedConnection), std::move(setupParameters));
  return std::make_unique<RSocketRequester>(std::move(rs), eventBase);
}

} // namespace rsocket
