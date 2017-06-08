// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketStats.h"

namespace rsocket {

class NoopStats : public RSocketStats {
 public:
  NoopStats() = default;
  ~NoopStats() = default;

  void socketCreated() override {}
  void socketDisconnected() override {}
  void socketClosed(StreamCompletionSignal signal) override {}

  void duplexConnectionCreated(
      const std::string& type,
      rsocket::DuplexConnection* connection) override {}
  void duplexConnectionClosed(
      const std::string& type,
      rsocket::DuplexConnection* connection) override {}

  void bytesWritten(size_t bytes) override {}
  void bytesRead(size_t bytes) override {}
  void frameWritten(FrameType frameType) override {}
  void frameRead(FrameType frameType) override {}

  void resumeBufferChanged(int, int) override {}
  void streamBufferChanged(int64_t, int64_t) override {}

  static std::shared_ptr<NoopStats> instance() {
    static auto singleton = std::make_shared<NoopStats>();
    return singleton;
  }

 private:
  NoopStats(const NoopStats& other) = delete; // non construction-copyable
  NoopStats& operator=(const NoopStats&) = delete; // non copyable
  NoopStats& operator=(const NoopStats&&) = delete; // non movable
  NoopStats(NoopStats&&) = delete; // non construction-movable
};

std::shared_ptr<RSocketStats> RSocketStats::noop() {
  return NoopStats::instance();
}
}
