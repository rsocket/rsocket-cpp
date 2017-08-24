// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/RSocketConnectionManager.h"

#include <folly/ExceptionWrapper.h>
#include <folly/ScopeGuard.h>
#include <folly/io/async/EventBase.h>

#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/statemachine/RSocketStateMachine.h"

namespace rsocket {

RSocketConnectionManager::~RSocketConnectionManager() {
  VLOG(1) << "Started ~RSocketConnectionManager";
  SCOPE_EXIT {
    VLOG(1) << "Finished ~RSocketConnectionManager";
  };

  StateMachineMap map;

  // Set up the baton and counter of connections yet to be closed.  Move all the
  // connections out of the synchronized map so we don't block while keeping the
  // map locked.
  {
    auto locked = sockets_.lock();
    if (locked->empty()) {
      VLOG(2) << "No connections to close, early exit";
      return;
    }

    shutdownBaton_.emplace();
    shutdownCounter_ = locked->size();

    VLOG(2) << "Need to close " << shutdownCounter_.load() << " connections";

    map.swap(*locked);
  }

  for (auto& kv : map) {
    auto rsocket = std::move(kv.first);
    auto& evb = kv.second;

    auto close = [rs = std::move(rsocket)] {
      rs->close({}, StreamCompletionSignal::SOCKET_CLOSED);
    };

    // We could be closing on the same thread as the state machine.  In that
    // case, close the state machine inline, otherwise we hang.
    if (evb.isInEventBaseThread()) {
      VLOG(3) << "Closing connection inline";
      close();
    } else {
      VLOG(3) << "Closing connection asynchronously";
      evb.runInEventBaseThread(close);
    }
  }

  DCHECK(sockets_.lock()->empty());

  VLOG(2) << "Blocking on shutdown baton";

  // Wait for all connections to close.
  shutdownBaton_->wait();

  VLOG(2) << "Unblocked off of shutdown baton";

  DCHECK(sockets_.lock()->empty());
  DCHECK_EQ(shutdownCounter_.load(), 0);
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
      connectionManager_.removeConnection(socket_);

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
  auto const result = locked->erase(socket);
  DCHECK_LE(result, 1);
  DCHECK(result == 1 || shutdownBaton_);

  VLOG(2) << "Removed RSocketStateMachine";

  if (shutdownBaton_) {
    auto const old = shutdownCounter_.fetch_sub(1);
    DCHECK_GT(old, 0);

    if (old == 1) {
      shutdownBaton_->post();
    }
  }
}
}
