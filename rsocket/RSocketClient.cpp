// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketClient.h"
#include "rsocket/RSocketRequester.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/framing/FrameTransport.h"
#include "rsocket/framing/FramedDuplexConnection.h"
#include "rsocket/internal/ClientResumeStatusCallback.h"
#include "rsocket/internal/FollyKeepaliveTimer.h"
#include "rsocket/internal/RSocketConnectionManager.h"

using namespace folly;

namespace rsocket {

RSocketClient::~RSocketClient() {
  VLOG(4) << "RSocketClient destroyed ..";
}

std::shared_ptr<RSocketRequester> RSocketClient::getRequester() const {
  return requester_;
}

RSocketClient::RSocketClient(
    std::unique_ptr<ConnectionFactory> connectionFactory,
    SetupParameters setupParameters,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    OnRSocketResume)
    : connectionFactory_(std::move(connectionFactory)),
      connectionManager_(std::make_unique<RSocketConnectionManager>()),
      setupParameters_(std::move(setupParameters)),
      responder_(std::move(responder)),
      keepaliveTimer_(std::move(keepaliveTimer)),
      stats_(stats),
      connectionEvents_(connectionEvents),
      resumeManager_(resumeManager),
      coldResumeHandler_(coldResumeHandler),
      protocolVersion_(setupParameters_.protocolVersion),
      token_(setupParameters_.token) {}

folly::Future<folly::Unit> RSocketClient::connect() {
  VLOG(2) << "Starting connection";

  folly::Promise<folly::Unit> promise;
  auto future = promise.getFuture();

  connectionFactory_->connect([ this, promise = std::move(promise) ](
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase & eventBase) mutable {
    VLOG(3) << "onConnect received DuplexConnection";
    fromConnection(std::move(connection), eventBase);
    promise.setValue();
  });

  return future;
}

folly::Future<folly::Unit> RSocketClient::resume() {
  VLOG(2) << "Resuming connection";

  CHECK(connectionFactory_)
      << "The client was likely created without ConnectionFactory. Can't "
      << "resume";

  // TODO: CHECK whether the underlying transport is closed before attempting
  // resumption.
  //
  folly::Promise<folly::Unit> promise;
  auto future = promise.getFuture();

  connectionFactory_->connect([ this, promise = std::move(promise) ](
      std::unique_ptr<DuplexConnection> connection,
      folly::EventBase & eventBase) mutable {

    CHECK(
        !evb_ /* cold-resumption */ ||
        evb_ == &eventBase /* warm-resumption */);

    class ResumeCallback : public ClientResumeStatusCallback {
     public:
      explicit ResumeCallback(folly::Promise<folly::Unit> promise)
          : promise_(std::move(promise)) {}

      void onResumeOk() noexcept override {
        promise_.setValue();
      }

      void onResumeError(folly::exception_wrapper ex) noexcept override {
        promise_.setException(ex);
      }
     private:
      folly::Promise<folly::Unit> promise_;
    };

    auto resumeCallback = std::make_unique<ResumeCallback>(std::move(promise));
    std::unique_ptr<DuplexConnection> framedConnection;
    if (connection->isFramed()) {
      framedConnection = std::move(connection);
    } else {
      framedConnection = std::make_unique<FramedDuplexConnection>(
          std::move(connection), protocolVersion_);
    }
    auto frameTransport =
        yarpl::make_ref<FrameTransport>(std::move(framedConnection));

    if (!stateMachine_) {
      createState(eventBase);
    }

    stateMachine_->tryClientResume(
        token_, std::move(frameTransport), std::move(resumeCallback));
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

void RSocketClient::fromConnection(
    std::unique_ptr<DuplexConnection> connection,
    folly::EventBase& eventBase) {
  evb_ = &eventBase;
  createState(eventBase);
  std::unique_ptr<DuplexConnection> framedConnection;
  if (connection->isFramed()) {
    framedConnection = std::move(connection);
  } else {
    framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), setupParameters_.protocolVersion);
  }
  stateMachine_->connectClientSendSetup(
      std::move(framedConnection), std::move(setupParameters_));
}

void RSocketClient::createState(folly::EventBase& eventBase) {
  CHECK(eventBase.isInEventBaseThread());

  // Creation of state is permitted only once for each RSocketClient.
  // When evb is removed from RSocketStateMachine, the state can be
  // created in constructor
  CHECK(!stateMachine_);

  stateMachine_ = std::make_shared<RSocketStateMachine>(
      eventBase,
      responder_,
      std::move(keepaliveTimer_),
      ReactiveSocketMode::CLIENT,
      stats_,
      connectionEvents_);

  requester_ = std::make_shared<RSocketRequester>(stateMachine_, eventBase);

  connectionManager_->manageConnection(stateMachine_, eventBase);
}

} // namespace rsocket
