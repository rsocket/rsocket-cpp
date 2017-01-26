// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <string>
#include "src/Common.h"
#include "src/DuplexConnection.h"
#include "src/Frame.h"

namespace reactivesocket {
class Stats {
 public:
  virtual ~Stats() = default;

  static Stats& noop();

  virtual void socketCreated() = 0;
  virtual void socketDisconnected() = 0;
  virtual void socketClosed(StreamCompletionSignal signal) = 0;

  virtual void duplexConnectionCreated(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) = 0;
  virtual void duplexConnectionClosed(
      const std::string& type,
      reactivesocket::DuplexConnection* connection) = 0;

  virtual void bytesWritten(size_t bytes) = 0;
  virtual void bytesRead(size_t bytes) = 0;
  virtual void frameWritten(const std::string& frameType) = 0;
  virtual void frameRead(const std::string& frameType) = 0;
  virtual void resumeBufferChanged(int framesCountDelta, int dataSizeDelta) = 0;
};
}
