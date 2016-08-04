// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <src/Stats.h>
#include <src/tcp/TcpDuplexConnection.h>

namespace reactivesocket {
class StatsPrinter : public Stats {
 public:
  void socketCreated() override;
  void socketClosed() override;
  virtual void connectionCreated(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override;
  virtual void connectionClosed(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override;
  virtual void bytesWritten(size_t bytes) override;
  virtual void bytesRead(size_t bytes) override;
  virtual void frameWritten(FrameType type) override;
  virtual void frameRead(FrameType type) override;
};
}
