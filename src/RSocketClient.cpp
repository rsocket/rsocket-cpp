// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RSocketClient.h"
#include "src/RSocketRequester.h"
#include "src/RSocketResponder.h"
#include "src/RSocketStats.h"
#include "src/framing/FrameTransport.h"
#include "src/framing/FramedDuplexConnection.h"
#include "src/internal/FollyKeepaliveTimer.h"
#include "src/internal/RSocketConnectionManager.h"

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

  CHECK(!stateMachine_);
  resumeToken_ = setupParameters.token;
  protocolVersion_ = setupParameters.protocolVersion;

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
    evb_ = &eventBase;
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

folly::Future<folly::Unit> RSocketClient::resume() {
  CHECK(stateMachine_);
  folly::Promise<folly::Unit> promise;
  auto future = promise.getFuture();
  connectionFactory_->connect([ this, promise = std::move(promise) ](
      std::unique_ptr<DuplexConnection> connection,
      bool isFramedConnection,
      folly::EventBase & evb) mutable {
    CHECK(evb_ == &evb);

    class ResumeCallback : public ClientResumeStatusCallback {
     public:
      ResumeCallback(folly::Promise<folly::Unit> promise)
          : promise_(std::move(promise)) {}
      void onResumeOk() noexcept override {
        promise_.setValue(folly::Unit());
      }
      void onResumeError(folly::exception_wrapper ex) noexcept override {
        promise_.setException(ex);
      }
      folly::Promise<folly::Unit> promise_;
    };

    auto resumeCallback = std::make_unique<ResumeCallback>(std::move(promise));

    std::unique_ptr<DuplexConnection> framedConnection;
    if (isFramedConnection) {
      framedConnection = std::move(connection);
    } else {
      framedConnection = std::make_unique<FramedDuplexConnection>(
          std::move(connection), protocolVersion_, evb);
    }

    auto frameTransport =
        std::make_shared<FrameTransport>(std::move(framedConnection));

    stateMachine_->tryClientResume(
        resumeToken_, std::move(frameTransport), std::move(resumeCallback));

  });

  return future;
}

void RSocketClient::disconnect(folly::exception_wrapper ex) {
  CHECK(stateMachine_);
  evb_->runInEventBaseThread([ this, ex = std::move(ex) ] {
    VLOG(2) << "Disconnecting RSocketStateMachine on EventBase";
    stateMachine_->disconnect(std::move(ex));
  });
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

  stateMachine_ = rs;

  connectionManager_->manageConnection(rs, eventBase, setupParameters.token);

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
