// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/framed/FramedReader.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(FramedReaderTest, Read1Frame) {
  auto frameSubscriber = makeMockSubscriber<std::unique_ptr<folly::IOBuf>>();
  auto wireSubscription = makeMockSubscription();

  std::string msg1("value1");

  auto payload1 = folly::IOBuf::create(0);
  folly::io::Appender a1(payload1.get(), 10);
  a1.writeBE<int32_t>(msg1.size() + sizeof(int32_t));
  folly::format("{}", msg1.c_str())(a1);

  auto framedReader = std::make_shared<FramedReader>(frameSubscriber);

  EXPECT_CALL(*frameSubscriber, onSubscribe_(_)).Times(1);

  framedReader->onSubscribe(wireSubscription);

  EXPECT_CALL(*frameSubscriber, onNext_(_)).Times(0);
  EXPECT_CALL(*frameSubscriber, onError_(_)).Times(0);
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(0);

  framedReader->onNext(std::move(payload1));

  EXPECT_CALL(*frameSubscriber, onNext_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
        ASSERT_EQ(msg1, p->moveToFbString().toStdString());
      }));

  EXPECT_CALL(*wireSubscription, request_(_)).Times(1);

  frameSubscriber->subscription()->request(3);

  // to delete objects
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(1);

  frameSubscriber->subscription()->cancel();
  framedReader->onComplete();
}

TEST(FramedReaderTest, Read3Frames) {
  auto frameSubscriber = makeMockSubscriber<std::unique_ptr<folly::IOBuf>>();
  auto wireSubscription = makeMockSubscription();

  std::string msg1("value1");
  std::string msg2("value2");
  std::string msg3("value3");

  auto payload1 = folly::IOBuf::create(0);
  folly::io::Appender a1(payload1.get(), 10);
  a1.writeBE<int32_t>(msg1.size() + sizeof(int32_t));
  folly::format("{}", msg1.c_str())(a1);
  a1.writeBE<int32_t>(msg2.size() + sizeof(int32_t));
  folly::format("{}", msg2.c_str())(a1);

  auto payload2 = folly::IOBuf::create(0);
  folly::io::Appender a2(payload2.get(), 10);
  a2.writeBE<int32_t>(msg3.size() + sizeof(int32_t));
  folly::format("{}", msg3.c_str())(a2);

  folly::IOBufQueue bufQueue;
  bufQueue.append(std::move(payload1));
  bufQueue.append(std::move(payload2));

  auto framedReader = std::make_shared<FramedReader>(frameSubscriber);

  EXPECT_CALL(*frameSubscriber, onSubscribe_(_)).Times(1);

  framedReader->onSubscribe(wireSubscription);

  EXPECT_CALL(*frameSubscriber, onNext_(_)).Times(0);
  EXPECT_CALL(*frameSubscriber, onError_(_)).Times(0);
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(0);

  framedReader->onNext(bufQueue.move());

  EXPECT_CALL(*frameSubscriber, onNext_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
        ASSERT_EQ(msg1, p->moveToFbString().toStdString());
      }))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
        ASSERT_EQ(msg2, p->moveToFbString().toStdString());
      }))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
        ASSERT_EQ(msg3, p->moveToFbString().toStdString());
      }));

  frameSubscriber->subscription()->request(3);

  // to delete objects
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(1);

  frameSubscriber->subscription()->cancel();
  framedReader->onComplete();
}

TEST(FramedReaderTest, Read1FrameIncomplete) {
  auto frameSubscriber = makeMockSubscriber<std::unique_ptr<folly::IOBuf>>();
  auto wireSubscription = makeMockSubscription();

  std::string part1("val");
  std::string part2("ueXXX");
  std::string msg1 = part1 + part2;

  auto framedReader = std::make_shared<FramedReader>(frameSubscriber);
  framedReader->onSubscribe(wireSubscription);

  EXPECT_CALL(*frameSubscriber, onNext_(_)).Times(0);
  EXPECT_CALL(*frameSubscriber, onError_(_)).Times(0);
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(0);

  frameSubscriber->subscription()->request(3);

  auto payload = folly::IOBuf::create(0);
  {
    folly::io::Appender appender(payload.get(), 10);
    appender.writeBE<int32_t>(msg1.size() + sizeof(int32_t));
  }

  framedReader->onNext(std::move(payload));

  payload = folly::IOBuf::create(0);
  {
    folly::io::Appender appender(payload.get(), 10);
    folly::format("{}", part1.c_str())(appender);
  }

  framedReader->onNext(std::move(payload));

  payload = folly::IOBuf::create(0);
  {
    folly::io::Appender appender(payload.get(), 10);
    folly::format("{}", part2.c_str())(appender);
  }

  EXPECT_CALL(*frameSubscriber, onNext_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
        ASSERT_EQ(msg1, p->moveToFbString().toStdString());
      }));

  framedReader->onNext(std::move(payload));
  // to delete objects
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(1);

  frameSubscriber->subscription()->cancel();
  framedReader->onComplete();
}
