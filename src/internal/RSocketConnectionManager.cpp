// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/internal/RSocketConnectionManager.h"
#include <folly/io/async/EventBase.h>
#include <folly/ScopeGuard.h>
#include <folly/ExceptionWrapper.h>
#include "src/statemachine/RSocketStateMachine.h"
#include "src/RSocketNetworkStats.h"

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
  auto onClose = [this, rs = socket, eventBase = &eventBase]() mutable {
      // Enqueue another event to remove and delete it.  We cannot delete
      // the RSocketStateMachine now as it still needs to finish processing
      // the onClosed handlers in the stack frame above us.
      eventBase->add([this, rs = std::move(rs)] {
          removeConnection(rs);
      });
  };

  class RemoveSocketNetworkStats : public RSocketNetworkStats, public decltype(onClose) {
   public:
    using Super = decltype(onClose);
    using Super::Super; // ctor from the lambda

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
      (*this)();
      if (inner) {
        inner->onClosed(ex);
      }
    }

    std::shared_ptr<RSocketNetworkStats> inner;
  };

  auto newNetworkStats = std::make_shared<RemoveSocketNetworkStats>(std::move(onClose));
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
