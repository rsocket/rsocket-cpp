// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Baton.h>
#include <folly/Optional.h>
#include <folly/Synchronized.h>

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
  void removeConnection(const std::shared_ptr<rsocket::RSocketStateMachine>&);

  /// Set of currently open ReactiveSockets.
  folly::Synchronized<
      std::unordered_map<
          std::shared_ptr<rsocket::RSocketStateMachine>,
          folly::EventBase&>,
      std::mutex>
      sockets_;

  folly::Optional<folly::Baton<>> shutdown_;
};
} // namespace rsocket
