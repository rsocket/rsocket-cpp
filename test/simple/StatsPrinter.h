// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <src/DuplexConnection.h>
#include <src/Stats.h>

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
  void frameWritten(const std::string& frameType) override;
  void frameRead(const std::string& frameType) override;
  void resumeBufferChanged(int framesCountDelta, int dataSizeDelta) override;

  void connectionClosedInServerConnectionAcceptor(
      const folly::exception_wrapper& ex) override;
};
}
