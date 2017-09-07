// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/RSocketConnectionManager.h"

#include <folly/ExceptionWrapper.h>
#include <folly/ScopeGuard.h>
#include <folly/io/async/EventBase.h>

#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/internal/Common.h"
#include "rsocket/internal/ManageableConnection.h"

namespace rsocket {

RSocketConnectionManager::~RSocketConnectionManager() {
  VLOG(1) << "Started ~RSocketConnectionManager";
  SCOPE_EXIT { VLOG(1) << "Finished ~RSocketConnectionManager"; };

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
    bool disconnectedOrClosed = rsocket->isDisconnectedOrClosed();

    auto close = [rs = std::move(rsocket)] {
      rs->close({}, StreamCompletionSignal::SOCKET_CLOSED);
    };

    // We could be closing on the same thread as the state machine.  In that
    // case, close the state machine inline, otherwise we hang.
    // If the rsocket is already disconnected, no need to defer the close()
    // to the evb, as the evb might already be deleted at this point.
    if (disconnectedOrClosed || evb.isInEventBaseThread()) {
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
  constexpr std::chrono::minutes kTimeout{1};
  if (!shutdownBaton_->timed_wait(kTimeout)) {
    LOG(ERROR) << "~RSocketConnectionManager timed out closing all connections";
  } else {
    VLOG(2) << "Unblocked off of shutdown baton";
  }

  DCHECK(sockets_.lock()->empty());
  DCHECK_EQ(shutdownCounter_.load(), 0);
}

void RSocketConnectionManager::manageConnection(
    std::shared_ptr<ManageableConnection> socket, folly::EventBase &eventBase) {
  auto future = socket->listenCloseEvent();
  // We assume this object will out live this future's lifetime.
  std::weak_ptr<ManageableConnection> weakSocket = socket;
  future.then([this, weakSocket]() {
    auto spSocket = weakSocket.lock();
    LOG_IF(ERROR, !spSocket)
        << "Connection manager has missed destruction of a connection.";
    removeConnection(spSocket);
  });

  sockets_.lock()->insert({std::move(socket), eventBase});
}

void RSocketConnectionManager::removeConnection(
    const std::shared_ptr<ManageableConnection> &socket) {
  auto locked = sockets_.lock();
  auto const result = locked->erase(socket);
  DCHECK_LE(result, 1);
  DCHECK(result == 1 || shutdownBaton_);

  VLOG(2) << "Removed ManageableConnection";

  if (shutdownBaton_) {
    auto const old = shutdownCounter_.fetch_sub(1);
    DCHECK_GT(old, 0);

    if (old == 1) {
      shutdownBaton_->post();
    }
  }
}
} // namespace rsocket
