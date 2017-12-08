// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/async/EventBase.h>

#include "rsocket/framing/Frame.h"

namespace rsocket {

class FrameSink {
 public:
  virtual ~FrameSink() = default;

  /// Terminates underlying connection sending the error frame
  /// on the connection.
  ///
  /// This may synchronously deliver terminal signals to all StreamAutomatonBase
  /// attached to this RSocketStateMachine.
  virtual void disconnectWithError(Frame_ERROR&&) = 0;

  virtual void sendKeepalive(
      std::unique_ptr<folly::IOBuf> data = folly::IOBuf::create(0)) = 0;
};

class KeepaliveTimer {
 public:
  KeepaliveTimer(std::chrono::milliseconds period, folly::EventBase& eventBase);

  ~KeepaliveTimer();

  std::chrono::milliseconds keepaliveTime();

  void schedule();

  void stop();

  void start(const std::shared_ptr<FrameSink>& connection);

  void sendKeepalive();

  void keepaliveReceived();

 private:
  std::shared_ptr<FrameSink> connection_;
  folly::EventBase& eventBase_;
  std::shared_ptr<uint32_t> generation_;
  std::chrono::milliseconds period_;
  std::atomic<bool> pending_{false};
};
}
