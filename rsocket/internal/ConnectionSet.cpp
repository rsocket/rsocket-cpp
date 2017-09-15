// Copyright 2004-present Facebook. All Rights Reserved.

#include "rsocket/internal/ConnectionSet.h"

#include "rsocket/statemachine/RSocketStateMachine.h"

#include <folly/io/async/EventBase.h>

namespace rsocket {

ConnectionSet::ConnectionSet() {}

ConnectionSet::~ConnectionSet() {
  VLOG(1) << "Started ~ConnectionSet";
  SCOPE_EXIT { VLOG(1) << "Finished ~ConnectionSet"; };

  StateMachineMap map;

  // Move all the connections out of the synchronized map so we don't block
  // while closing the state machines.
  {
    auto locked = machines_.lock();
    if (locked->empty()) {
      VLOG(2) << "No connections to close, early exit";
      return;
    }

    map.swap(*locked);
  }

  VLOG(2) << "Need to close " << map.size() << " connections";

  for (auto& kv : map) {
    auto rsocket = std::move(kv.first);
    auto evb = kv.second;

    auto close = [rs = std::move(rsocket)] {
      rs->close({}, StreamCompletionSignal::SOCKET_CLOSED);
    };

    // We could be closing on the same thread as the state machine.  In that
    // case, close the state machine inline, otherwise we hang.
    if (evb->isInEventBaseThread()) {
      VLOG(3) << "Closing connection inline";
      close();
    } else {
      VLOG(3) << "Closing connection asynchronously";
      evb->runInEventBaseThread(close);
    }
  }
}

void ConnectionSet::insert(
    std::shared_ptr<RSocketStateMachine> machine,
    folly::EventBase* evb) {
  machines_.lock()->emplace(std::move(machine), evb);
}

void ConnectionSet::remove(
    const std::shared_ptr<RSocketStateMachine>& machine) {
  auto locked = machines_.lock();
  auto const result = locked->erase(machine);
  DCHECK_LE(result, 1);
}
}
