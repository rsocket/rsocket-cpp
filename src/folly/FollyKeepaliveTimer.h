// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/futures/ScheduledExecutor.h>
#include <src/ConnectionAutomaton.h>
#include <src/ReactiveSocket.h>

namespace reactivesocket {
class FollyKeepaliveTimer : public KeepaliveTimer {
 public:
  FollyKeepaliveTimer(
      folly::ScheduledExecutor& executor,
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
  folly::ScheduledExecutor& executor_;
  std::shared_ptr<bool> running_;
  std::chrono::milliseconds period_;
  std::atomic<bool> pending_{false};
};
}
