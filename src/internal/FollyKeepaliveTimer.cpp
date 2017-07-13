// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/internal/FollyKeepaliveTimer.h"

namespace rsocket {

FollyKeepaliveTimer::FollyKeepaliveTimer(
    folly::EventBase& eventBase,
    std::chrono::milliseconds period)
    : eventBase_(eventBase),
      generation_(std::make_shared<uint32_t>(0)),
      period_(period) {}

FollyKeepaliveTimer::~FollyKeepaliveTimer() {
  stop();
}

std::chrono::milliseconds FollyKeepaliveTimer::keepaliveTime() {
  return period_;
}

void FollyKeepaliveTimer::schedule() {
  auto scheduledGeneration = *generation_;
  auto generation = generation_;
  eventBase_.runAfterDelay(
      [this, generation, scheduledGeneration]() {
        if (*generation == scheduledGeneration) {
          sendKeepalive();
        }
      },
      static_cast<uint32_t>(keepaliveTime().count()));
}

void FollyKeepaliveTimer::sendKeepalive() {
  if (pending_) {
    // Make sure connection_ is not deleted (via external call to stop)
    // while we still mid-operation
    auto localPtr = connection_;
    stop();
    // TODO: we need to use max lifetime from the setup frame for this
    localPtr->disconnectOrCloseWithError(
        Frame_ERROR::connectionError("no response to keepalive"));
  } else {
    connection_->sendKeepalive();
    pending_ = true;
    schedule();
  }
}

// must be called from the same thread as start
void FollyKeepaliveTimer::stop() {
  *generation_ += 1;
  pending_ = false;
  connection_ = nullptr;
}

// must be called from the same thread as stop
void FollyKeepaliveTimer::start(const std::shared_ptr<FrameSink>& connection) {
  connection_ = connection;
  *generation_ += 1;
  DCHECK(!pending_);

  schedule();
}

void FollyKeepaliveTimer::keepaliveReceived() {
  pending_ = false;
}
}
