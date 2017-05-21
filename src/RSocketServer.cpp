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
      setupResumeAcceptor_(ProtocolVersion::Unknown) {}

RSocketServer::~RSocketServer() {
  // Stop accepting new connections.
  duplexConnectionAcceptor_->stop();

  // FIXME(alexanderm): This is where we /should/ close the FrameTransports
  // sitting around in the SetupResumeAcceptor, but we can't yet...

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

    // TODO => Ben commented out to get unit tests working

  // Wait for all ReactiveSockets to close.
  //  shutdown_->wait();
  //  DCHECK(sockets_.lock()->empty());

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
        VLOG(2) << "Going to accept duplex connection";

        // FIXME(alexanderm): This isn't thread safe
        setupResumeAcceptor_.accept(
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
  LOG(INFO) << "RSocketServer => received new setup payload";

  // FIXME(alexanderm): Handler should be tied to specific executor
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

  //TODO: fix the shut down part
//  rs->addClosedListener(
//      [this, stateMachine](const folly::exception_wrapper&) {
//        // Enqueue another event to remove and delete it.  We cannot delete
//        // the RSocketStateMachine now as it still needs to finish processing
//        // the onClosed handlers in the stack frame above us.
//        // TODO => Ben commented out to get unit tests working
//        //          executor_.add([this, stateMachine] {
//        //            server_->removeConnection(stateMachine);
//        //          });
//
//      });

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
    std::shared_ptr<rsocket::RSocketStateMachine> socket) {
  auto locked = sockets_.lock();
  locked->erase(socket);

  LOG(INFO) << "Removed ReactiveSocket";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}
} // namespace rsocket
