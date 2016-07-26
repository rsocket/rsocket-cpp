// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/Stats.h"
#include <src/tcp/TcpDuplexConnection.h>

namespace reactivesocket {

class NoopStats : public Stats {
 public:
  void socketCreated() override{};
  void socketClosed() override{};
  void connectionCreated(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override{};
  void connectionClosed(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override{};
  void bytesWritten(size_t bytes) override{};
  void bytesRead(size_t bytes) override{};
  void frameWritten(FrameType type) override{};
  void frameRead(FrameType type) override{};
};

Stats& Stats::noop() {
  static NoopStats noop_;
  return noop_;
};
}
