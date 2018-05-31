// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/io/IOBuf.h>

#include "rsocket/DuplexConnection.h"
#include "rsocket/framing/FrameProcessor.h"

namespace rsocket {

// Refer to FrameTransportImpl for documentation on the implementation
class FrameTransport {
 public:
  virtual ~FrameTransport() = default;
  virtual void setFrameProcessor(std::shared_ptr<FrameProcessor>) = 0;
  virtual void outputFrameOrDrop(std::unique_ptr<folly::IOBuf>) = 0;
  virtual void close() = 0;

  // Just for observation purposes!
  // TODO(T25011919): remove
  virtual DuplexConnection* getConnection() = 0;

  virtual bool isConnectionFramed() const = 0;
};
} // namespace rsocket
