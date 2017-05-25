// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketServer.h"
#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseManager.h>
#include "RSocketErrors.h"
#include "statemachine/RSocketStateMachine.h"
#include "internal/ScheduledRSocketResponder.h"
#include "framing/FrameTransport.h"
#include "RSocketStats.h"

using namespace rsocket;

namespace rsocket {

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : duplexConnectionAcceptor_(std::move(connectionAcceptor)),
      setupResumeAcceptors_([]{return new rsocket::SetupResumeAcceptor(ProtocolVersion::Unknown); }) {}

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

  // Asynchronously close all existing ReactiveSockets.  If there are none, then
  // we can do an early exit.
  {
    auto locked = sockets_.lock();
    if (locked->empty()) {
      return;
    }

    shutdown_.emplace();

    for (auto& connectionPair : *locked) {
      // close() has to be called on the same executor as the socket
      auto& executor_ = connectionPair.second;
      executor_.add([s = connectionPair.first] {
        s->close(
            folly::exception_wrapper(), StreamCompletionSignal::SOCKET_CLOSED);
      });
    }
  }

  // Wait for all ReactiveSockets to close.
  shutdown_->wait();
  DCHECK(sockets_.lock()->empty());

  // All requests are fully finished, worker threads can be safely killed off.
}

void RSocketServer::start(OnSetupConnection onSetupConnection) {
  if (started) {
    throw std::runtime_error("RSocketServer::start() already called.");
  }
  started = true;

  LOG(INFO) << "Initializing connection acceptor on start";

  duplexConnectionAcceptor_
      ->start([ this, onSetupConnection = std::move(onSetupConnection) ](
          std::unique_ptr<DuplexConnection> connection, folly::EventBase& eventBase) {

        auto* acceptor = setupResumeAcceptors_.get();

        if (isShutdown_) {
          // connection is getting out of scope and terminated
          //
          // if we created a new acceptor its ok, we aren't queuing anything new
          return;
        }
        VLOG(2) << "Going to accept duplex connection";

        acceptor->accept(
            std::move(connection),
            std::bind(&RSocketServer::onSetupConnection, this,
                      std::move(onSetupConnection), std::placeholders::_1,
                      std::placeholders::_2),
            std::bind(&RSocketServer::onResumeConnection, this,
                      OnResumeConnection(), std::placeholders::_1,
                      std::placeholders::_2));
      })
      .get(); // block until finished and return or throw
}

void RSocketServer::onSetupConnection(
    OnSetupConnection onSetupConnection,
    std::shared_ptr<rsocket::FrameTransport> frameTransport,
    rsocket::SetupParameters setupParams) {
  // we don't need to check for isShutdown_ here since all callbacks are
  // processed by this time

  LOG(INFO) << "RSocketServer => received new setup payload";

  auto* eventBase = folly::EventBaseManager::get()->getExistingEventBase();

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

  auto rs = std::make_shared<RSocketStateMachine>(
      *eventBase,
      std::move(requestResponder),
      RSocketStats::noop(),
      nullptr,
      ReactiveSocketMode::SERVER);

  rs->addClosedListener(
      [this, rs, eventBase](const folly::exception_wrapper&) {
        // Enqueue another event to remove and delete it.  We cannot delete
        // the RSocketStateMachine now as it still needs to finish processing
        // the onClosed handlers in the stack frame above us.
        eventBase->add([this, rs] {
          removeConnection(rs);
        });

      });

  addConnection(rs, *eventBase);

  // TODO ---> this code needs to be moved inside RSocketStateMachine

  // Connect last, after all state has been set up.
  rs->setResumable(setupParams.resumable);

  if (setupParams.protocolVersion != ProtocolVersion::Unknown) {
    rs->setFrameSerializer(
        FrameSerializer::createFrameSerializer(setupParams.protocolVersion));
  }

  rs->connect(std::move(frameTransport), true, setupParams.protocolVersion);

  // TODO <---- up to here
  // TODO and then a simple API such as:
  // TODO rs->connectServer(frameTransport, params)
}

void RSocketServer::onResumeConnection(
    OnResumeConnection onResumeConnection,
    std::shared_ptr<rsocket::FrameTransport> frameTransport,
    rsocket::ResumeParameters setupPayload) {
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

void RSocketServer::addConnection(
    std::shared_ptr<rsocket::RSocketStateMachine> socket,
    folly::Executor& executor) {
  sockets_.lock()->insert({std::move(socket), executor});
}

void RSocketServer::removeConnection(
    const std::shared_ptr<rsocket::RSocketStateMachine>& socket) {
  auto locked = sockets_.lock();
  locked->erase(socket);

  LOG(INFO) << "Removed ReactiveSocket";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}
} // namespace rsocket
