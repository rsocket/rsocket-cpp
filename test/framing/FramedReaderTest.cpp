// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include "rsocket/framing/FramedReader.h"
#include "test/test_utils/MockDuplexConnection.h"

using namespace rsocket;
using namespace testing;
using namespace yarpl::mocks;

TEST(FramedReader, TinyFrame) {
  auto version = std::make_shared<ProtocolVersion>(ProtocolVersion::Latest);
  auto reader = yarpl::make_ref<FramedReader>(version);

  // Not using hex string-literal as std::string ctor hits '\x00' and stops
  // reading.
  auto buf = folly::IOBuf::createCombined(4);
  buf->append(4);
  buf->writableData()[0] = '\x00';
  buf->writableData()[1] = '\x00';
  buf->writableData()[2] = '\x00';
  buf->writableData()[3] = '\x02';

  reader->onSubscribe(yarpl::flowable::Subscription::empty());
  reader->onNext(std::move(buf));

  auto subscriber = yarpl::make_ref<
      StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onError_(_));

  reader->setInput(subscriber);
  subscriber->awaitTerminalEvent();
  reader->onComplete();
}

TEST(FramedReader, CantDetectVersion) {
  auto version = std::make_shared<ProtocolVersion>(ProtocolVersion::Unknown);
  auto reader = yarpl::make_ref<FramedReader>(version);

  auto buf = folly::IOBuf::copyBuffer("ABCDEFGHIJKLMNOP");

  reader->onSubscribe(yarpl::flowable::Subscription::empty());
  reader->onNext(std::move(buf));

  auto subscriber = yarpl::make_ref<
      StrictMock<MockSubscriber<std::unique_ptr<folly::IOBuf>>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onError_(_));

  reader->setInput(subscriber);
  subscriber->awaitTerminalEvent();
  reader->onComplete();
}
