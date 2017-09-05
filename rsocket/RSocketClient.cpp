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

const std::shared_ptr<RSocketRequester>& RSocketClient::getRequester() const {
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

  return connectionFactory_->connect().then([this](
      ConnectionFactory::ConnectedDuplexConnection connection) mutable {
    VLOG(3) << "onConnect received DuplexConnection";

    // fromConnection method must be called from the eventBase and since
    // there is no guarantee that the Future returned from the factory::connect
    // method is executed on the event base, we have to ensure it by using
    // folly::via
    return via(&connection.eventBase).then([this, connection = std::move(
        connection.connection), eventBase = &connection.eventBase]() mutable {
      fromConnection(std::move(connection), *eventBase);
    });
  });
}

folly::Future<folly::Unit> RSocketClient::resume() {
  VLOG(2) << "Resuming connection";

  CHECK(connectionFactory_)
      << "The client was likely created without ConnectionFactory. Can't "
      << "resume";

  // TODO: CHECK whether the underlying transport is closed before attempting
  // resumption.
  //
  return connectionFactory_->connect().then([this](
      ConnectionFactory::ConnectedDuplexConnection connection) mutable {

    if (!evb_) {
      // cold-resumption
      evb_ = &connection.eventBase;
    } else {
      // warm-resumption
      CHECK(evb_ == &connection.eventBase);
    }

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

    folly::Promise<folly::Unit> promise;
    auto future = promise.getFuture();

    auto resumeCallback = std::make_unique<ResumeCallback>(std::move(promise));
    std::unique_ptr<DuplexConnection> framedConnection;
    if (connection.connection->isFramed()) {
      framedConnection = std::move(connection.connection);
    } else {
      framedConnection = std::make_unique<FramedDuplexConnection>(
          std::move(connection.connection), protocolVersion_);
    }
    auto frameTransport =
        yarpl::make_ref<FrameTransport>(std::move(framedConnection));

    connection.eventBase.runInEventBaseThread([
      this,
      frameTransport = std::move(frameTransport),
      resumeCallback = std::move(resumeCallback),
      connection = std::move(connection)
    ]() mutable {
      if (!stateMachine_) {
        createState(connection.eventBase);
      }

      stateMachine_->tryClientResume(
          token_,
          std::move(frameTransport),
          std::move(resumeCallback),
          protocolVersion_);
    });

    return future;

  });
}

void RSocketClient::disconnect(folly::exception_wrapper ex) {
  CHECK(stateMachine_);
  evb_->runInEventBaseThread([ this, ex = std::move(ex) ]() mutable {
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
  CHECK(!stateMachine_) << "A stateMachine has already been created";

  if (!keepaliveTimer_) {
    keepaliveTimer_ = std::make_unique<FollyKeepaliveTimer>(
        eventBase, std::chrono::seconds(5));
  }

  if (!responder_) {
    responder_ = std::make_shared<RSocketResponder>();
  }

  stateMachine_ = std::make_shared<RSocketStateMachine>(
      std::move(responder_),
      std::move(keepaliveTimer_),
      RSocketMode::CLIENT,
      std::move(stats_),
      std::move(connectionEvents_),
      std::move(resumeManager_),
      std::move(coldResumeHandler_));

  requester_ = std::make_shared<RSocketRequester>(stateMachine_, eventBase);

  connectionManager_->manageConnection(stateMachine_, eventBase);
}

} // namespace rsocket
