// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/futures/ManualExecutor.h>
#include <folly/io/Cursor.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <src/folly/FollyKeepaliveTimer.h>
#include "src/framed/FramedDuplexConnection.h"

using namespace ::testing;
using namespace ::reactivesocket;

namespace {
class MockConnectionAutomaton : public FrameSink {
 public:
  void sendKeepalive() override {}

  void disconnectWithError(Frame_ERROR&& error) override {}
};
}

TEST(FollyKeepaliveTimerTest, StartStop) {
  auto connectionAutomaton =
      std::make_shared<StrictMock<MockConnectionAutomaton>>();

  folly::ManualExecutor manualExecutor;

  FollyKeepaliveTimer timer(manualExecutor, std::chrono::milliseconds(100));

  timer.start(connectionAutomaton);

  timer.stop();
}