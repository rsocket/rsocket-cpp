// Copyright 2004-present Facebook. All Rights Reserved.

#include "FollyKeepaliveTimer.h"

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
          if (pending_) {
            stop();

            connection_->disconnectWithError(
                Frame_ERROR::connectionError("no response to keepalive"));
          } else {
            connection_->sendKeepalive();
            pending_ = true;
            schedule();
          }
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

void FollyKeepaliveTimer::keepaliveReceived() {
  pending_ = false;
}
}
