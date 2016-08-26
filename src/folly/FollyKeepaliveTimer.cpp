// Copyright 2004-present Facebook. All Rights Reserved.

#include "FollyKeepaliveTimer.h"
#include <folly/io/async/EventBase.h>
#include <folly/io/IOBuf.h>
#include <src/ConnectionAutomaton.h>
#include <src/ReactiveSocket.h>

namespace reactivesocket {
FollyKeepaliveTimer::FollyKeepaliveTimer(
    folly::EventBase& eventBase,
    std::chrono::milliseconds period)
    : eventBase_(eventBase), period_(period) {
  running_ = std::make_shared<bool>(false);
};

FollyKeepaliveTimer::~FollyKeepaliveTimer() {
  stop();
}

std::chrono::milliseconds FollyKeepaliveTimer::keepaliveTime() {
  return period_;
}

void FollyKeepaliveTimer::schedule() {
  auto running = running_;
  eventBase_.runAfterDelay(
      [this, running]() {
        if (*running) {
          automaton_->sendKeepalive();
          schedule();
        }
      },
      keepaliveTime().count());
}

// must be called from the thread as start
void FollyKeepaliveTimer::stop() {
  *running_ = false;
}

// must be called from the thread as stop
void FollyKeepaliveTimer::start(ConnectionAutomaton* automaton) {
  automaton_ = automaton;
  *running_ = true;

  schedule();
}
}
