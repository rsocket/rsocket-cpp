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

RSocketClient::RSocketClient(
    std::shared_ptr<ConnectionFactory> connectionFactory,
    ProtocolVersion protocolVersion,
    ResumeIdentificationToken token,
    std::shared_ptr<RSocketResponder> responder,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    std::shared_ptr<RSocketStats> stats,
    std::shared_ptr<RSocketConnectionEvents> connectionEvents,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler)
    : connectionFactory_(std::move(connectionFactory)),
      connectionManager_(std::make_unique<RSocketConnectionManager>()),
      responder_(std::move(responder)),
      keepaliveTimer_(std::move(keepaliveTimer)),
      stats_(stats),
      connectionEvents_(connectionEvents),
      resumeManager_(resumeManager),
      coldResumeHandler_(coldResumeHandler),
      protocolVersion_(protocolVersion),
      token_(std::move(token)) {}

RSocketClient::~RSocketClient() {
  VLOG(4) << "RSocketClient destroyed ..";
  disconnect().get();
}

const std::shared_ptr<RSocketRequester>& RSocketClient::getRequester() const {
  return requester_;
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

folly::Future<folly::Unit> RSocketClient::disconnect(
    folly::exception_wrapper ew) {
  if (!stateMachine_) {
    return folly::makeFuture<folly::Unit>(
        std::runtime_error{"RSocketClient must always have a state machine"});
  }

  auto work = [ sm = stateMachine_, e = std::move(ew) ]() mutable {
    sm->disconnect(std::move(e));
  };

  if (evb_->isInEventBaseThread()) {
    VLOG(2) << "Running RSocketClient disconnect synchronously";
    work();
    return folly::unit;
  }

  VLOG(2) << "Scheduling RSocketClient disconnect";
  return folly::via(evb_, work);
}

void RSocketClient::fromConnection(
    std::unique_ptr<DuplexConnection> connection,
    folly::EventBase& eventBase,
    SetupParameters setupParameters
) {
  evb_ = &eventBase;
  createState(eventBase);
  std::unique_ptr<DuplexConnection> framedConnection;
  if (connection->isFramed()) {
    framedConnection = std::move(connection);
  } else {
    framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), setupParameters.protocolVersion);
  }
  stateMachine_->connectClientSendSetup(
      std::move(framedConnection), std::move(setupParameters));
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
