// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include "src/framing/FrameTransport.h"
#include "test/test_utils/MockDuplexConnection.h"
#include "test/test_utils/MockFrameProcessor.h"

using namespace rsocket;
using namespace testing;

namespace {

/*
 * Compare a `const folly::IOBuf&` against a `const std::string&`.
 */
MATCHER_P(IOBufStringEq, s, "") {
  return folly::IOBufEqual()(*arg, *folly::IOBuf::copyBuffer(s));
}

/*
 * Make a MockDuplexConnection, and run an arbitrary lambda on the subscriber it
 * returns from getOutput().
 */
template <class SubscriberFn>
std::unique_ptr<StrictMock<MockDuplexConnection>> makeConnection(
    SubscriberFn fn) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();

  EXPECT_CALL(*connection, setInput_(_));
  EXPECT_CALL(*connection, getOutput_()).WillOnce(Invoke([&] {
    auto subscriber = yarpl::make_ref<
        StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
    fn(subscriber);
    return subscriber;
  }));

  return connection;
}
}

TEST(FrameTransport, Close) {
  auto connection = makeConnection([](auto& subscriber) {
    EXPECT_CALL(*subscriber, onSubscribe_(_));
    EXPECT_CALL(*subscriber, onComplete_());
  });

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));
  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());
  transport->close();
}

TEST(FrameTransport, CloseWithError) {
  auto connection = makeConnection([](auto& subscriber) {
    EXPECT_CALL(*subscriber, onSubscribe_(_));
    EXPECT_CALL(*subscriber, onError_(_));
  });

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));
  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());
  transport->closeWithError(std::runtime_error("Uh oh"));
}

TEST(FrameTransport, SimpleEnqueue) {
  auto connection = makeConnection([](auto& subscriber) {
    EXPECT_CALL(*subscriber, onSubscribe_(_));

    EXPECT_CALL(*subscriber, onNext_(IOBufStringEq("Hello")));
    EXPECT_CALL(*subscriber, onNext_(IOBufStringEq("World")));

    EXPECT_CALL(*subscriber, onComplete_());
  });

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));

  transport->outputFrameOrEnqueue(folly::IOBuf::copyBuffer("Hello"));
  transport->outputFrameOrEnqueue(folly::IOBuf::copyBuffer("World"));

  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());
  transport->close();
}

TEST(FrameTransport, SimpleNoQueue) {
  auto connection = makeConnection([](auto& subscriber) {
    EXPECT_CALL(*subscriber, onSubscribe_(_));

    EXPECT_CALL(*subscriber, onNext_(IOBufStringEq("Hello")));
    EXPECT_CALL(*subscriber, onNext_(IOBufStringEq("World")));

    EXPECT_CALL(*subscriber, onComplete_());
  });

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));

  transport->setFrameProcessor(
      std::make_shared<StrictMock<MockFrameProcessor>>());

  transport->outputFrameOrEnqueue(folly::IOBuf::copyBuffer("Hello"));
  transport->outputFrameOrEnqueue(folly::IOBuf::copyBuffer("World"));

  transport->close();
}

TEST(FrameTransport, InputSendsError) {
  auto connection = makeConnection([](auto& subscriber) {
    EXPECT_CALL(*subscriber, onSubscribe_(_));
    EXPECT_CALL(*subscriber, onComplete_());
  });

  ON_CALL(*connection, setInput_(_)).WillByDefault(Invoke([](auto& subscriber) {
    auto subscription = yarpl::make_ref<StrictMock<MockSubscription>>();
    EXPECT_CALL(*subscription, request_(_));
    EXPECT_CALL(*subscription, cancel_());

    subscriber->onSubscribe(std::move(subscription));
    subscriber->onError(std::make_exception_ptr(std::runtime_error("Oops")));
  }));

  auto transport = yarpl::make_ref<FrameTransport>(std::move(connection));

  auto processor = std::make_shared<StrictMock<MockFrameProcessor>>();
  EXPECT_CALL(*processor, onTerminal_(_));

  transport->setFrameProcessor(std::move(processor));
  transport->close();
}
