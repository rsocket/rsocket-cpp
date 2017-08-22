// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketRequester.h"

namespace folly {
class EventBase;
}

namespace rsocket {

class RSocketServerState {
 public:
  void close();

  std::shared_ptr<RSocketRequester> getRequester() {
    return rSocketRequester_;
  }

  friend class RSocketServer;

 private:
  RSocketServerState(
      folly::EventBase& eventBase,
      std::shared_ptr<RSocketStateMachine> stateMachine,
      std::shared_ptr<RSocketRequester> rSocketRequester)
      : eventBase_(eventBase),
        rSocketStateMachine_(stateMachine),
        rSocketRequester_(rSocketRequester) {}

  folly::EventBase& eventBase_;
  std::shared_ptr<RSocketStateMachine> rSocketStateMachine_;
  std::shared_ptr<RSocketRequester> rSocketRequester_;
};
}
