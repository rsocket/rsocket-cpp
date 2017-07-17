// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <stdint.h>
#include <string>
#include "rsocket/internal/Common.h"

namespace rsocket {

class DuplexConnection;

class RSocketStats {
 public:
  virtual ~RSocketStats() = default;

  static std::shared_ptr<RSocketStats> noop();

  virtual void socketCreated() = 0;
  virtual void socketDisconnected() = 0;
  virtual void socketClosed(StreamCompletionSignal signal) = 0;

  virtual void duplexConnectionCreated(
      const std::string& type,
      DuplexConnection* connection) = 0;
  virtual void duplexConnectionClosed(
      const std::string& type,
      DuplexConnection* connection) = 0;

  virtual void bytesWritten(size_t bytes) = 0;
  virtual void bytesRead(size_t bytes) = 0;
  virtual void frameWritten(FrameType frameType) = 0;
  virtual void frameRead(FrameType frameType) = 0;
  virtual void resumeBufferChanged(int framesCountDelta, int dataSizeDelta) = 0;
  virtual void streamBufferChanged(
      int64_t framesCountDelta,
      int64_t dataSizeDelta) = 0;
};
}
