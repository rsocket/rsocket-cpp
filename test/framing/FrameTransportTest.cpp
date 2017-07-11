// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include "src/framing/FrameTransport.h"
#include "test/test_utils/MockDuplexConnection.h"
#include "test/test_utils/MockFrameProcessor.h"

using namespace rsocket;
using namespace testing;

TEST(FrameTransport, Close) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();

  EXPECT_CALL(*connection, setInput_(_));
  EXPECT_CALL(*connection, getOutput_()).WillOnce(Invoke([&] {
    auto subscriber = yarpl::make_ref<
        StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
    EXPECT_CALL(*subscriber, onSubscribe_(_));
    EXPECT_CALL(*subscriber, onComplete_());
    return subscriber;
  }));

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));
  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());
  transport->close();
}

TEST(FrameTransport, CloseWithError) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();

  EXPECT_CALL(*connection, setInput_(_));
  EXPECT_CALL(*connection, getOutput_()).WillOnce(Invoke([&] {
    auto subscriber = yarpl::make_ref<
        StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
    EXPECT_CALL(*subscriber, onSubscribe_(_));
    EXPECT_CALL(*subscriber, onError_(_));
    return subscriber;
  }));

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));
  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());
  transport->closeWithError(std::runtime_error("Uh oh"));
}
