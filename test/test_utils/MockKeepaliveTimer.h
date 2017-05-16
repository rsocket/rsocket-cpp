// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <chrono>
#include <memory>

#include <gmock/gmock.h>

#include "src/statemachine/RSocketStateMachine.h"
#include "src/temporary_home/ReactiveSocket.h"

namespace reactivesocket {
class MockKeepaliveTimer : public KeepaliveTimer {
 public:
  MOCK_METHOD1(start, void(const std::shared_ptr<FrameSink>&));
  MOCK_METHOD0(stop, void());
  MOCK_METHOD0(keepaliveReceived, void());
  MOCK_METHOD0(keepaliveTime, std::chrono::milliseconds());
};
}
