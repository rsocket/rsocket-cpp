// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/RSocketRequester.h"

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
      std::shared_ptr<RSocketStateMachine> stateMachine,
      std::shared_ptr<RSocketRequester> rSocketRequester)
      : rSocketStateMachine_(stateMachine),
        rSocketRequester_(rSocketRequester) {}
  std::shared_ptr<RSocketStateMachine> rSocketStateMachine_;
  std::shared_ptr<RSocketRequester> rSocketRequester_;
};
}
