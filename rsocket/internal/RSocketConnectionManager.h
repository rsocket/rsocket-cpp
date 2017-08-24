// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Baton.h>
#include <folly/Optional.h>
#include <folly/Synchronized.h>

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace folly {
class EventBase;
}

namespace rsocket {

class RSocketStateMachine;

class RSocketConnectionManager {
 public:
  ~RSocketConnectionManager();

  void manageConnection(
      std::shared_ptr<rsocket::RSocketStateMachine>,
      folly::EventBase&);

 private:
  using StateMachineMap = std::unordered_map<
      std::shared_ptr<rsocket::RSocketStateMachine>,
      folly::EventBase&>;

  void removeConnection(const std::shared_ptr<rsocket::RSocketStateMachine>&);

  folly::Synchronized<StateMachineMap, std::mutex> sockets_;

  folly::Optional<folly::Baton<>> shutdownBaton_;
  std::atomic<size_t> shutdownCounter_{0};
};
}
