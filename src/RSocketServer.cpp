// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/RSocketServer.h"
#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>
#include "framing/FrameTransport.h"
#include "internal/ScheduledRSocketResponder.h"
#include "src/RSocketErrors.h"
#include "src/RSocketNetworkStats.h"
#include "src/RSocketStats.h"
#include "statemachine/RSocketStateMachine.h"
#include "src/internal/RSocketConnectionManager.h"

namespace rsocket {

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : duplexConnectionAcceptor_(std::move(connectionAcceptor)),
      setupResumeAcceptors_([]{
        return new rsocket::SetupResumeAcceptor(
            ProtocolVersion::Unknown,
            folly::EventBaseManager::get()->getExistingEventBase());
      }),
      connectionManager_(std::make_unique<RSocketConnectionManager>()) {}

RSocketServer::~RSocketServer() {
  // Will stop forwarding connections from duplexConnectionAcceptor_ to
  // setupResumeAcceptors_
  isShutdown_ = true;

  // Stop accepting new connections.
  duplexConnectionAcceptor_->stop();

  std::vector<folly::Future<folly::Unit>> closingFutures;
  for (auto& acceptor : setupResumeAcceptors_.accessAllThreads()) {
    // this call will queue up the cleanup on the eventBase
    closingFutures.push_back(acceptor.close());
  }

  folly::collectAll(closingFutures).get();

  connectionManager_.reset(); // will close all existing RSockets and wait

  // All requests are fully finished, worker threads can be safely killed off.
}

void RSocketServer::start(OnSetupConnection onSetupConnection) {
  if (started) {
    throw std::runtime_error("RSocketServer::start() already called.");
  }
  started = true;

  LOG(INFO) << "Starting RSocketServer";

  duplexConnectionAcceptor_
      ->start([ this, onSetupConnection = std::move(onSetupConnection) ](
          std::unique_ptr<DuplexConnection> connection,
          folly::EventBase & eventBase) {
        if (isShutdown_) {
          // connection is getting out of scope and terminated
          return;
        }

        auto* acceptor = setupResumeAcceptors_.get();

        VLOG(2) << "Going to accept duplex connection";

        acceptor->accept(
            std::move(connection),
            std::bind(
                &RSocketServer::onSetupConnection,
                this,
                std::move(onSetupConnection),
                std::placeholders::_1,
                std::placeholders::_2),
            std::bind(
                &RSocketServer::onResumeConnection,
                this,
                OnResumeConnection(),
                std::placeholders::_1,
                std::placeholders::_2));
      })
      .get(); // block until finished and return or throw
}

// this class will be moved, its just an intermediate step
class RSocketServerNetworkStats : public RSocketNetworkStats {
 public:
  void onClosed(const folly::exception_wrapper&) override {
    if (onClose) {
      onClose();
    }
  }

   std::function<void()> onClose;
};

void RSocketServer::onSetupConnection(
    OnSetupConnection onSetupConnection,
    std::shared_ptr<FrameTransport> frameTransport,
    SetupParameters setupParams) {
  // we don't need to check for isShutdown_ here since all callbacks are
  // processed by this time
  VLOG(1) << "Received new setup payload";

  auto* eventBase = folly::EventBaseManager::get()->getExistingEventBase();
  CHECK(eventBase);

  std::shared_ptr<RSocketResponder> requestResponder;
  try {
    requestResponder = onSetupConnection(setupParams);
  } catch (const RSocketError& e) {
    // TODO emit ERROR ... but how do I do that here?
    frameTransport->close(
        folly::exception_wrapper{std::current_exception(), e});
    return;
  }

  if(requestResponder) {
    requestResponder = std::make_shared<ScheduledRSocketResponder>(
        std::move(requestResponder), *eventBase);
  } else {
    // if the responder was not provided, we will create a default one
    requestResponder = std::make_shared<RSocketResponder>();
  }

  auto removeRSocketCallback = std::make_shared<RSocketServerNetworkStats>();

  auto rs = std::make_shared<RSocketStateMachine>(
      *eventBase,
      std::move(requestResponder),
      nullptr,
      ReactiveSocketMode::SERVER,
      RSocketStats::noop(),
      removeRSocketCallback);

  connectionManager_->manageConnection(rs, *eventBase);
  rs->connectServer(std::move(frameTransport), setupParams);
}

void RSocketServer::onResumeConnection(
    OnResumeConnection onResumeConnection,
    std::shared_ptr<FrameTransport> frameTransport,
    ResumeParameters setupPayload) {
  // we don't need to check for isShutdown_ here since all callbacks are
  // processed by this time

  CHECK(false) << "not implemented";
}

void RSocketServer::startAndPark(OnSetupConnection onSetupConnection) {
  start(std::move(onSetupConnection));
  waiting_.wait();
}

void RSocketServer::unpark() {
  waiting_.post();
}

} // namespace rsocket
