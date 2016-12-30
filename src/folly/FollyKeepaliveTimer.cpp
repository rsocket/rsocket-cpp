// Copyright 2004-present Facebook. All Rights Reserved.

#include "FollyKeepaliveTimer.h"

namespace reactivesocket {

FollyKeepaliveTimer::FollyKeepaliveTimer(
    folly::EventBase& eventBase,
    std::chrono::milliseconds period)
    : eventBase_(eventBase),
      running_(std::make_shared<bool>(false)),
      period_(period) {}

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
          sendKeepalive();
          schedule();
        }
      },
      keepaliveTime().count());
}

void FollyKeepaliveTimer::sendKeepalive() {
  if (pending_) {
    stop();
    connection_->closeWithError(
        Frame_ERROR::connectionError("no response to keepalive"));
  } else {
    connection_->sendKeepalive();
    pending_ = true;
  }
}

// must be called from the same thread as start
void FollyKeepaliveTimer::stop() {
  *running_ = false;
  pending_ = false;
}

// must be called from the same thread as stop
void FollyKeepaliveTimer::start(const std::shared_ptr<FrameSink>& connection) {
  connection_ = connection;
  *running_ = true;
  DCHECK(!pending_);

  schedule();
}

void FollyKeepaliveTimer::keepaliveReceived() {
  pending_ = false;
}
}
