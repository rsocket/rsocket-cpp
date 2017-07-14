// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <mutex>
#include <unordered_map>
#include <folly/Baton.h>
#include <folly/Optional.h>
#include <folly/Synchronized.h>

#include "Common.h"

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
      folly::EventBase&,
      ResumeIdentificationToken);

  std::shared_ptr<rsocket::RSocketStateMachine> getConnection(
      ResumeIdentificationToken) const;

 private:
  void removeConnection(const std::shared_ptr<rsocket::RSocketStateMachine>&);

  /// Set of currently open ReactiveSockets.
  folly::Synchronized<
      std::unordered_map<
          std::shared_ptr<rsocket::RSocketStateMachine>,
          std::pair<folly::EventBase&, ResumeIdentificationToken>>,
      std::mutex>
      sockets_;

  folly::Optional<folly::Baton<>> shutdown_;
};
} // namespace rsocket
