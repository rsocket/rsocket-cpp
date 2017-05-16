// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <src/DuplexConnection.h>
#include <src/temporary_home/Stats.h>

namespace reactivesocket {
class StatsPrinter : public Stats {
 public:
  void socketCreated() override;
  void socketClosed(StreamCompletionSignal signal) override;
  void socketDisconnected() override;

  void duplexConnectionCreated(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override;
  void duplexConnectionClosed(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) override;

  void bytesWritten(size_t bytes) override;
  void bytesRead(size_t bytes) override;
  void frameWritten(FrameType frameType) override;
  void frameRead(FrameType frameType) override;
  void resumeBufferChanged(int framesCountDelta, int dataSizeDelta) override;
  void streamBufferChanged(int64_t framesCountDelta, int64_t dataSizeDelta)
      override;
};
}
