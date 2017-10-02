// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/RSocketClient.h"
#include "rsocket/RSocketRequester.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/RSocketStats.h"
#include "rsocket/framing/FrameTransportImpl.h"
#include "rsocket/framing/FramedDuplexConnection.h"
#include "rsocket/framing/ScheduledFrameTransport.h"
#include "rsocket/internal/ClientResumeStatusCallback.h"
#include "rsocket/internal/ConnectionSet.h"
#include "rsocket/internal/FollyKeepaliveTimer.h"

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
      connectionSet_(std::make_shared<ConnectionSet>()),
      responder_(std::move(responder)),
      keepaliveTimer_(std::move(keepaliveTimer)),
      stats_(stats),
      connectionEvents_(connectionEvents),
      resumeManager_(resumeManager),
      coldResumeHandler_(coldResumeHandler),
      protocolVersion_(protocolVersion),
      token_(std::move(token)) {}

RSocketClient::~RSocketClient() {
  VLOG(3) << "RSocketClient destroyed ..";
  disconnect().get();
}

const std::shared_ptr<RSocketRequester>& RSocketClient::getRequester() const {
  return requester_;
}

folly::Future<folly::Unit> RSocketClient::resume(folly::EventBase* smEvb) {
  VLOG(2) << "Resuming connection";

  CHECK(connectionFactory_)
      << "The client was likely created without ConnectionFactory. Can't "
      << "resume";

  if (smEvb) {
    if (evb_ /* warm-resumption */ && evb_ != smEvb) {
      throw ResumptionException(
          "The requested EventBase is different from the EventBase the"
          "client is already using");
    } else { // cold-resumption.  Use the given evb for stateMachine.
      evb_ = smEvb;
    }
  }

  return connectionFactory_->connect().then([this](
      ConnectionFactory::ConnectedDuplexConnection connection) mutable {

    if (!evb_) {
      // cold-resumption.  EventBase hasn't been explicitly set for SM by the
      // application.  Use the transport's eventBase.
      evb_ = &connection.eventBase;
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
    auto transport =
        yarpl::make_ref<FrameTransportImpl>(std::move(framedConnection));

    yarpl::Reference<FrameTransport> ft;
    if (evb_ != &connection.eventBase) {
      // If the StateMachine EventBase is different from the transport
      // EventBase, then use ScheduledFrameTransport and
      // ScheduledFrameProcessor to ensure the RSocketStateMachine and
      // Transport live on the desired EventBases
      ft = yarpl::make_ref<ScheduledFrameTransport>(
          std::move(transport),
          &connection.eventBase, /* Transport EventBase */
          evb_); /* StateMachine EventBase */
    } else {
      ft = std::move(transport);
    }

    evb_->runInEventBaseThread([
      this,
      frameTransport = std::move(ft),
      resumeCallback = std::move(resumeCallback),
      connection = std::move(connection)
    ]() mutable {
      if (!stateMachine_) {
        createState();
      }

      stateMachine_->resumeClient(
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
    folly::EventBase* smEvb,
    folly::EventBase& transportEvb,
    SetupParameters setupParameters) {
  if (smEvb) {
    evb_ = smEvb;
  } else {
    // If no EventBase is given for the stateMachine, then use the transport's
    // EventBase to drive the stateMachine.
    evb_ = &transportEvb;
  }
  createState();
  std::unique_ptr<DuplexConnection> framedConnection;
  if (connection->isFramed()) {
    framedConnection = std::move(connection);
  } else {
    framedConnection = std::make_unique<FramedDuplexConnection>(
        std::move(connection), setupParameters.protocolVersion);
  }
  auto transport =
      yarpl::make_ref<FrameTransportImpl>(std::move(framedConnection));
  if (evb_ != &transportEvb) {
    // If the StateMachine EventBase is different from the transport
    // EventBase, then use ScheduledFrameTransport and ScheduledFrameProcessor
    // to ensure the RSocketStateMachine and Transport live on the desired
    // EventBases
    auto scheduledFT = yarpl::make_ref<ScheduledFrameTransport>(
        std::move(transport),
        &transportEvb, /* Transport EventBase */
        evb_); /* StateMachine EventBase */
    evb_->runInEventBaseThread([
      stateMachine = stateMachine_,
      scheduledFT = std::move(scheduledFT),
      setupParameters = std::move(setupParameters)
    ]() mutable {
      stateMachine->connectClient(
          std::move(scheduledFT), std::move(setupParameters));
    });
  } else {
    stateMachine_->connectClient(
        std::move(transport), std::move(setupParameters));
  }
}

void RSocketClient::createState() {
  // Creation of state is permitted only once for each RSocketClient.
  // When evb is removed from RSocketStateMachine, the state can be
  // created in constructor
  CHECK(!stateMachine_) << "A stateMachine has already been created";

  if (!keepaliveTimer_) {
    keepaliveTimer_ =
        std::make_unique<FollyKeepaliveTimer>(*evb_, std::chrono::seconds(5));
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

  requester_ = std::make_shared<RSocketRequester>(stateMachine_, *evb_);

  connectionSet_->insert(stateMachine_, evb_);
  stateMachine_->registerSet(connectionSet_);
}

} // namespace rsocket
