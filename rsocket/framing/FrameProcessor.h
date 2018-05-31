// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>

namespace rsocket {

class FrameProcessor {
 public:
  virtual ~FrameProcessor() = default;

  virtual void processFrame(std::unique_ptr<folly::IOBuf>) = 0;
  virtual void onTerminal(folly::exception_wrapper) = 0;
};

} // namespace rsocket
