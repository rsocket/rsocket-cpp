// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/internal/Common.h"

namespace folly {
class IOBuf;
class exception_wrapper;
}

namespace rsocket {

class FrameProcessor {
 public:
  virtual ~FrameProcessor() = default;

  virtual void processFrame(std::unique_ptr<folly::IOBuf>) = 0;
  virtual void onTerminal(folly::exception_wrapper) = 0;
};

} // reactivesocket
