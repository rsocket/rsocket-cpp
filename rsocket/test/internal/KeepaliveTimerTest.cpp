// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/io/Cursor.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rsocket/framing/Frame.h"
#include "rsocket/framing/FramedDuplexConnection.h"
#include "rsocket/internal/KeepaliveTimer.h"

using namespace ::testing;
using namespace ::rsocket;

namespace {
class MockConnectionAutomaton : public FrameSink {
 public:
  // MOCK_METHOD doesn't take functions with unique_ptr args.
  // A workaround for sendKeepalive method.
  virtual void sendKeepalive(std::unique_ptr<folly::IOBuf> b) override {
    sendKeepalive_(b);
  }
  MOCK_METHOD1(sendKeepalive_, void(std::unique_ptr<folly::IOBuf>&));

  MOCK_METHOD1(disconnectOrCloseWithError_, void(Frame_ERROR&));

  void disconnectOrCloseWithError(Frame_ERROR&& error) override {
    disconnectOrCloseWithError_(error);
  }
};
} // namespace

TEST(FollyKeepaliveTimerTest, StartStopWithResponse) {
  auto connectionAutomaton =
      std::make_shared<NiceMock<MockConnectionAutomaton>>();

  EXPECT_CALL(*connectionAutomaton, sendKeepalive_(_)).Times(2);

  folly::EventBase eventBase;

  KeepaliveTimer timer(std::chrono::milliseconds(100), eventBase);

  timer.start(connectionAutomaton);

  timer.sendKeepalive();

  timer.keepaliveReceived();

  timer.sendKeepalive();

  timer.stop();
}

TEST(FollyKeepaliveTimerTest, NoResponse) {
  auto connectionAutomaton =
      std::make_shared<StrictMock<MockConnectionAutomaton>>();

  EXPECT_CALL(*connectionAutomaton, sendKeepalive_(_)).Times(1);
  EXPECT_CALL(*connectionAutomaton, disconnectOrCloseWithError_(_)).Times(1);

  folly::EventBase eventBase;

  KeepaliveTimer timer(std::chrono::milliseconds(100), eventBase);

  timer.start(connectionAutomaton);

  timer.sendKeepalive();

  timer.sendKeepalive();

  timer.stop();
}
