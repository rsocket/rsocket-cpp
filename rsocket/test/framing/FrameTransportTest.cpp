// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include "rsocket/framing/FrameTransportImpl.h"
#include "rsocket/test/test_utils/MockDuplexConnection.h"
#include "rsocket/test/test_utils/MockFrameProcessor.h"

using namespace rsocket;
using namespace testing;

namespace {

/*
 * Compare a `const folly::IOBuf&` against a `const std::string&`.
 */
MATCHER_P(IOBufStringEq, s, "") {
  return folly::IOBufEqual()(*arg, *folly::IOBuf::copyBuffer(s));
}

} // namespace

TEST(FrameTransport, Close) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  EXPECT_CALL(*connection, setInput_(_));

  auto transport = yarpl::make_ref<FrameTransportImpl>(std::move(connection));
  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());
  transport->close();
}

TEST(FrameTransport, SimpleNoQueue) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  EXPECT_CALL(*connection, setInput_(_));

  EXPECT_CALL(*connection, send_(IOBufStringEq("Hello")));
  EXPECT_CALL(*connection, send_(IOBufStringEq("World")));

  auto transport = yarpl::make_ref<FrameTransportImpl>(std::move(connection));

  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());

  transport->outputFrameOrDrop(folly::IOBuf::copyBuffer("Hello"));
  transport->outputFrameOrDrop(folly::IOBuf::copyBuffer("World"));

  transport->close();
}

TEST(FrameTransport, InputSendsError) {
  auto connection =
      std::make_unique<StrictMock<MockDuplexConnection>>([](auto input) {
        auto subscription =
            yarpl::make_ref<StrictMock<yarpl::mocks::MockSubscription>>();
        EXPECT_CALL(*subscription, request_(_));
        EXPECT_CALL(*subscription, cancel_());

        input->onSubscribe(std::move(subscription));
        input->onError(std::runtime_error("Oops"));
      });

  auto transport = yarpl::make_ref<FrameTransportImpl>(std::move(connection));

  auto processor = std::make_shared<StrictMock<MockFrameProcessor>>();
  EXPECT_CALL(*processor, onTerminal_(_));

  transport->setFrameProcessor(std::move(processor));
  transport->close();
}
