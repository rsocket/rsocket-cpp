// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/internal/RSocketConnectionManager.h"

#include <folly/io/async/EventBase.h>
#include <folly/ScopeGuard.h>
#include <folly/ExceptionWrapper.h>

#include "src/RSocketNetworkStats.h"
#include "src/statemachine/RSocketStateMachine.h"

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
  class RemoveSocketNetworkStats : public RSocketNetworkStats {
   public:
    RemoveSocketNetworkStats(
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

    RSocketConnectionManager& connectionManager_;
    std::shared_ptr<RSocketStateMachine> socket_;
    folly::EventBase& eventBase_;

    std::shared_ptr<RSocketNetworkStats> inner;
  };

  auto newNetworkStats = std::make_shared<RemoveSocketNetworkStats>(
      *this, socket, eventBase);
  newNetworkStats->inner = std::move(socket->networkStats());
  socket->networkStats() = std::move(newNetworkStats);

  sockets_.lock()->insert({std::move(socket), eventBase});
}

void RSocketConnectionManager::removeConnection(
    const std::shared_ptr<RSocketStateMachine>& socket) {
  auto locked = sockets_.lock();
  locked->erase(socket);

  VLOG(2) << "Removed ReactiveSocket";

  if (shutdown_ && locked->empty()) {
    shutdown_->post();
  }
}
} // namespace rsocket
