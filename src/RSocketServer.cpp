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

namespace rsocket {

RSocketServer::RSocketServer(
    std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
    : duplexConnectionAcceptor_(std::move(connectionAcceptor)),
      setupResumeAcceptors_([]{
        return new rsocket::SetupResumeAcceptor(
            ProtocolVersion::Unknown,
            folly::EventBaseManager::get()->getExistingEventBase());
      }) {}

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

  removeRSocketCallback->onClose = [this, rs, eventBase]() {
      // Enqueue another event to remove and delete it.  We cannot delete
      // the RSocketStateMachine now as it still needs to finish processing
      // the onClosed handlers in the stack frame above us.
      eventBase->add([this, rs] {
          removeConnection(rs);
      });
  };

  addConnection(rs, *eventBase);
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

void RSocketServer::addConnection(
    std::shared_ptr<RSocketStateMachine> socket,
    folly::Executor& executor) {
  sockets_.lock()->insert({std::move(socket), executor});
}

void RSocketServer::removeConnection(
    const std::shared_ptr<RSocketStateMachine>& socket) {
  auto locked = sockets_.lock();
  locked->erase(socket);

  VLOG(2) << "Removed ReactiveSocket";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}
} // namespace rsocket
