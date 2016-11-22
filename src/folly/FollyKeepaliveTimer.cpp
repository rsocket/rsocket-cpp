// Copyright 2004-present Facebook. All Rights Reserved.

#include "FollyKeepaliveTimer.h"
#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
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
          connection_->sendKeepalive();
          schedule();
        }
      },
      keepaliveTime().count());
}

// must be called from the same thread as start
void FollyKeepaliveTimer::stop() {
  *running_ = false;
}

// must be called from the same thread as stop
void FollyKeepaliveTimer::start(
    const std::shared_ptr<ConnectionAutomaton>& connection) {
  connection_ = connection;
  *running_ = true;

  schedule();
}
}
