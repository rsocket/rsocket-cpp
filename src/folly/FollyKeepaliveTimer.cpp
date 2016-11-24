// Copyright 2004-present Facebook. All Rights Reserved.

#include "FollyKeepaliveTimer.h"
#include <folly/io/IOBuf.h>
#include <folly/io/async/EventBase.h>
#include <src/ConnectionAutomaton.h>
#include <src/ReactiveSocket.h>

namespace reactivesocket {
FollyKeepaliveTimer::FollyKeepaliveTimer(
    folly::ScheduledExecutor& executor,
    std::chrono::milliseconds period)
    : executor_(executor), period_(period) {
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
  executor_.schedule(
      [this, running]() {
        if (*running) {
          sendKeepalive();

          if (*running) {
            schedule();
          }
        }
      },
      keepaliveTime());
}

void FollyKeepaliveTimer::sendKeepalive() {
  if (pending_) {
    stop();

    connection_->disconnectWithError(
        Frame_ERROR::connectionError("no response to keepalive"));
  } else {
    connection_->sendKeepalive();
    pending_ = true;
  }
}

// must be called from the same thread as start
void FollyKeepaliveTimer::stop() {
  *running_ = false;
}

// must be called from the same thread as stop
void FollyKeepaliveTimer::start(const std::shared_ptr<FrameSink>& connection) {
  connection_ = connection;
  *running_ = true;

  schedule();
}

void FollyKeepaliveTimer::keepaliveReceived() {
  pending_ = false;
}
}
