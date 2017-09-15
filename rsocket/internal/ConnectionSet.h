// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Synchronized.h>

#include <memory>
#include <mutex>
#include <unordered_map>

namespace folly {
class EventBase;
}

namespace rsocket {

class RSocketStateMachine;

/// Set of RSocketStateMachine objects.  Stores them until they call
/// RSocketStateMachine::close().
///
/// Also tracks which EventBase is controlling each state machine so that they
/// can be closed on the correct thread.
class ConnectionSet {
 public:
  ConnectionSet();
  ~ConnectionSet();

  void insert(std::shared_ptr<RSocketStateMachine>, folly::EventBase*);

  void remove(const std::shared_ptr<RSocketStateMachine>&);

 private:
  using StateMachineMap = std::
      unordered_map<std::shared_ptr<RSocketStateMachine>, folly::EventBase*>;

  folly::Synchronized<StateMachineMap, std::mutex> machines_;
};
}
