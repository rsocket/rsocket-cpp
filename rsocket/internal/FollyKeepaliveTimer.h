// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>

#include "rsocket/statemachine/RSocketStateMachine.h"

namespace rsocket {

class FollyKeepaliveTimer : public KeepaliveTimer {
 public:
  FollyKeepaliveTimer(
      folly::EventBase& eventBase,
      std::chrono::milliseconds period);

  ~FollyKeepaliveTimer();

  std::chrono::milliseconds keepaliveTime() override;

  void schedule();

  void stop() override;

  void start(const std::shared_ptr<FrameSink>& connection) override;

  void sendKeepalive();

  void keepaliveReceived() override;

 private:
  std::shared_ptr<FrameSink> connection_;
  folly::EventBase& eventBase_;
  std::shared_ptr<uint32_t> generation_;
  std::chrono::milliseconds period_;
  std::atomic<bool> pending_{false};
};
}
