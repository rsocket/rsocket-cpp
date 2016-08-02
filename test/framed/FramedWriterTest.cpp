// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/ExceptionWrapper.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/Payload.h"
#include "src/framed/FramedWriter.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(FramedWriterTest, Subscribe) {
  auto& subscriber = makeMockSubscriber<Payload>();
  auto& subscription = makeMockSubscription();

  EXPECT_CALL(subscriber, onSubscribe_(_)).Times(1);
  EXPECT_CALL(subscription, cancel_()).Times(1);

  FramedWriter writer(subscriber, Stats::noop());
  writer.onSubscribe(subscription);

  // to delete objects
  subscriber.subscription()->cancel();
  writer.onComplete();
}

TEST(FramedWriterTest, Error) {
  auto& subscriber = makeMockSubscriber<Payload>();
  auto& subscription = makeMockSubscription();

  FramedWriter writer(subscriber, Stats::noop());

  EXPECT_CALL(subscription, cancel_()).Times(1);
  writer.onSubscribe(subscription);

  // calls passed thru
  EXPECT_CALL(subscriber, onError_(_)).Times(1);
  writer.onError(std::runtime_error("error1"));

  //  subscriber.subscription()->cancel();
}

TEST(FramedWriterTest, Complete) {
  auto& subscriber = makeMockSubscriber<Payload>();
  auto& subscription = makeMockSubscription();

  FramedWriter writer(subscriber, Stats::noop());

  EXPECT_CALL(subscription, cancel_()).Times(1);
  writer.onSubscribe(subscription);

  // calls passed thru
  EXPECT_CALL(subscriber, onComplete_()).Times(1);
  writer.onComplete();

  //  subscriber.subscription()->cancel();
}

static void nextSingleFrameTest(int headroom) {
  auto& subscriber = makeMockSubscriber<Payload>();
  auto& subscription = makeMockSubscription();

  EXPECT_CALL(subscriber, onError_(_)).Times(0);
  EXPECT_CALL(subscriber, onComplete_()).Times(0);
  EXPECT_CALL(subscription, cancel_()).Times(0);

  std::string msg("hello");

  EXPECT_CALL(subscriber, onNext_(_)).WillOnce(Invoke([&](Payload& p) {
    ASSERT_EQ(
        folly::to<std::string>(
            '\0', '\0', '\0', char(msg.size() + sizeof(int32_t)), msg),
        p->moveToFbString().toStdString());
  }));

  FramedWriter writer(subscriber, Stats::noop());
  writer.onSubscribe(subscription);
  writer.onNext(folly::IOBuf::copyBuffer(msg, headroom));

  // to delete objects
  EXPECT_CALL(subscriber, onComplete_()).Times(1);
  EXPECT_CALL(subscription, cancel_()).Times(1);

  subscriber.subscription()->cancel();
  writer.onComplete();
}

TEST(FramedWriterTest, NextSingleFrameNoHeadroom) {
  nextSingleFrameTest(0);
}

TEST(FramedWriterTest, NextSingleFrameWithHeadroom) {
  nextSingleFrameTest(sizeof(int32_t));
}

static void nextTwoFramesTest(int headroom) {
  auto& subscriber = makeMockSubscriber<Payload>();
  auto& subscription = makeMockSubscription();

  EXPECT_CALL(subscriber, onError_(_)).Times(0);
  EXPECT_CALL(subscriber, onComplete_()).Times(0);
  EXPECT_CALL(subscription, cancel_()).Times(0);

  std::string msg1("hello");
  std::string msg2("world");

  folly::IOBuf payloadChain;

  EXPECT_CALL(subscriber, onNext_(_))
      .WillOnce(
          Invoke([&](Payload& p) { payloadChain.prependChain(std::move(p)); }))
      .WillOnce(Invoke([&](Payload& p) {
        payloadChain.prependChain(std::move(p));
        ASSERT_EQ(
            folly::to<std::string>(
                '\0',
                '\0',
                '\0',
                char(msg1.size() + sizeof(int32_t)),
                msg1,
                '\0',
                '\0',
                '\0',
                char(msg2.size() + sizeof(int32_t)),
                msg2),
            payloadChain.moveToFbString().toStdString());
      }));

  FramedWriter writer(subscriber, Stats::noop());
  writer.onSubscribe(subscription);
  writer.onNext(folly::IOBuf::copyBuffer(msg1, headroom));
  writer.onNext(folly::IOBuf::copyBuffer(msg2, headroom));

  // to delete objects
  EXPECT_CALL(subscriber, onComplete_()).Times(1);
  EXPECT_CALL(subscription, cancel_()).Times(1);

  subscriber.subscription()->cancel();
  writer.onComplete();
}

TEST(FramedWriterTest, NextTwoFramesNoHeadroom) {
  nextTwoFramesTest(0);
}

TEST(FramedWriterTest, NextTwoFramesWithHeadroom) {
  nextTwoFramesTest(sizeof(int32_t));
}
