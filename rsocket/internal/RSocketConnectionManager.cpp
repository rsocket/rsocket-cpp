// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/RSocketConnectionManager.h"

#include <folly/io/async/EventBase.h>
#include <folly/ScopeGuard.h>
#include <folly/ExceptionWrapper.h>

#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/statemachine/RSocketStateMachine.h"

namespace rsocket {

RSocketConnectionManager::~RSocketConnectionManager() {
  // Asynchronously close all existing ReactiveSockets.  If there are none, then
  // we can do an early exit.
  VLOG(1) << "Destroying RSocketConnectionManager...";
  auto scopeGuard = folly::makeGuard([]{ VLOG(1) << "Destroying RSocketConnectionManager... DONE"; });

  {
    auto locked = sockets_.lock();
    if (locked->empty()) {
      return;
    }

    shutdown_.emplace();

    for (auto& connectionPair : *locked) {
      // close() has to be called on the same executor as the socket
      auto& executor_ = connectionPair.second;
      executor_.add([rs = std::move(connectionPair.first)] {
        rs->close(
            folly::exception_wrapper(), StreamCompletionSignal::SOCKET_CLOSED);
      });
    }
  }

  // Wait for all ReactiveSockets to close.
  shutdown_->wait();
  DCHECK(sockets_.lock()->empty());
}

void RSocketConnectionManager::manageConnection(
    std::shared_ptr<RSocketStateMachine> socket,
    folly::EventBase& eventBase) {
  class ConnectionEventsWrapper : public RSocketConnectionEvents {
   public:
    ConnectionEventsWrapper(
        RSocketConnectionManager& connectionManager,
        std::shared_ptr<RSocketStateMachine> socket,
        folly::EventBase& eventBase)
        : connectionManager_(connectionManager),
          socket_(std::move(socket)),
          eventBase_(eventBase) {}

    void onConnected() override {
      if (inner) {
        inner->onConnected();
      }
    }

    void onDisconnected(const folly::exception_wrapper& ex) override {
      if (inner) {
        inner->onDisconnected(ex);
      }
    }

    void onClosed(const folly::exception_wrapper& ex) override {
      // Enqueue another event to remove and delete it.  We cannot delete
      // the RSocketStateMachine now as it still needs to finish processing
      // the onClosed handlers in the stack frame above us.
      eventBase_.add([connectionManager = &connectionManager_, socket = std::move(socket_)] {
        connectionManager->removeConnection(socket);
      });

      if (inner) {
        inner->onClosed(ex);
      }
    }

    void onStreamsPaused() override {
      if (inner) {
        inner->onStreamsPaused();
      }
    }

    void onStreamsResumed() override {
      if (inner) {
        inner->onStreamsResumed();
      }
    }

    RSocketConnectionManager& connectionManager_;
    std::shared_ptr<RSocketStateMachine> socket_;
    folly::EventBase& eventBase_;

    std::shared_ptr<RSocketConnectionEvents> inner;
  };

  auto connectionEventsWrapper =
      std::make_shared<ConnectionEventsWrapper>(*this, socket, eventBase);
  connectionEventsWrapper->inner = std::move(socket->connectionEvents());
  socket->connectionEvents() = std::move(connectionEventsWrapper);

  sockets_.lock()->insert({std::move(socket), eventBase});
}

void RSocketConnectionManager::removeConnection(
    const std::shared_ptr<RSocketStateMachine>& socket) {
  auto locked = sockets_.lock();
  locked->erase(socket);

  VLOG(2) << "Removed RSocketStateMachine";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}
} // namespace rsocket
