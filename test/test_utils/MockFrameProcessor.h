// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gmock/gmock.h>

#include <folly/io/IOBuf.h>
#include <folly/ExceptionWrapper.h>

#include "src/framing/FrameProcessor.h"

namespace rsocket {

class MockFrameProcessor : public FrameProcessor {
public:
  void processFrame(std::unique_ptr<folly::IOBuf> buf) override {
    processFrame_(buf);
  }

  void onTerminal(folly::exception_wrapper ew) override {
    onTerminal_(std::move(ew));
  }

  MOCK_METHOD1(processFrame_, void(std::unique_ptr<folly::IOBuf>&));
  MOCK_METHOD1(onTerminal_, void(folly::exception_wrapper));
};

}
