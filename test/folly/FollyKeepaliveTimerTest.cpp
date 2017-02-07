// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/io/Cursor.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <src/Frame.h>
#include <src/folly/FollyKeepaliveTimer.h>
#include "src/framed/FramedDuplexConnection.h"

using namespace ::testing;
using namespace ::reactivesocket;

namespace {
class MockConnectionAutomaton : public FrameSink {
 public:
  MOCK_METHOD0(sendKeepalive, void());
  MOCK_METHOD1(disconnectOrCloseWithError_, void(Frame_ERROR&));

  void disconnectOrCloseWithError(Frame_ERROR&& error) override {
    disconnectOrCloseWithError_(error);
  }
};
}

TEST(FollyKeepaliveTimerTest, StartStopWithResponse) {
  auto connectionAutomaton =
      std::make_shared<NiceMock<MockConnectionAutomaton>>();

  EXPECT_CALL(*connectionAutomaton, sendKeepalive()).Times(2);

  folly::EventBase eventBase;

  FollyKeepaliveTimer timer(eventBase, std::chrono::milliseconds(100));

  timer.start(connectionAutomaton);

  timer.sendKeepalive();

  timer.keepaliveReceived();

  timer.sendKeepalive();

  timer.stop();
}

TEST(FollyKeepaliveTimerTest, NoResponse) {
  auto connectionAutomaton =
      std::make_shared<StrictMock<MockConnectionAutomaton>>();

  EXPECT_CALL(*connectionAutomaton, sendKeepalive()).Times(1);
  EXPECT_CALL(*connectionAutomaton, disconnectOrCloseWithError_(_)).Times(1);

  folly::EventBase eventBase;

  FollyKeepaliveTimer timer(eventBase, std::chrono::milliseconds(100));

  timer.start(connectionAutomaton);

  timer.sendKeepalive();

  timer.sendKeepalive();

  timer.stop();
}