// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>
#include <src/ConnectionAutomaton.h>
#include <src/ReactiveSocket.h>

namespace reactivesocket {
class FollyKeepaliveTimer : public KeepaliveTimer {
 public:
  FollyKeepaliveTimer(
      folly::EventBase& eventBase,
      std::chrono::milliseconds period);

  ~FollyKeepaliveTimer();

  std::chrono::milliseconds keepaliveTime() override;

  void schedule();

  void stop() override;

  void start(const std::shared_ptr<ConnectionAutomaton>& connection) override;

 private:
  std::shared_ptr<ConnectionAutomaton> connection_;
  folly::EventBase& eventBase_;
  std::shared_ptr<bool> running_;
  std::chrono::milliseconds period_;
};
}
