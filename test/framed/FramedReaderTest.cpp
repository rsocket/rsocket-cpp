// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/io/Cursor.h>
#include <gmock/gmock.h>
#include "src/StandardReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/framed/FramedReader.h"
#include "test/InlineConnection.h"
#include "test/MockRequestHandler.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(FramedReaderTest, Read1Frame) {
  auto frameSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  auto wireSubscription = std::make_shared<MockSubscription>();

  std::string msg1("value1");

  auto payload1 = folly::IOBuf::create(0);
  folly::io::Appender a1(payload1.get(), 10);
  a1.writeBE<int32_t>(msg1.size() + sizeof(int32_t));
  folly::format("{}", msg1.c_str())(a1);

  auto framedReader =
      std::make_shared<FramedReader>(frameSubscriber, inlineExecutor());

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
  EXPECT_CALL(*wireSubscription, cancel_()).Times(1);

  frameSubscriber->subscription()->cancel();
  framedReader->onComplete();
}

TEST(FramedReaderTest, Read3Frames) {
  auto frameSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  auto wireSubscription = std::make_shared<MockSubscription>();

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

  auto framedReader =
      std::make_shared<FramedReader>(frameSubscriber, inlineExecutor());

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
  EXPECT_CALL(*wireSubscription, cancel_()).Times(1);

  frameSubscriber->subscription()->cancel();
  framedReader->onComplete();
}

TEST(FramedReaderTest, Read1FrameIncomplete) {
  auto frameSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  auto wireSubscription = std::make_shared<MockSubscription>();

  std::string part1("val");
  std::string part2("ueXXX");
  std::string msg1 = part1 + part2;

  auto framedReader =
      std::make_shared<FramedReader>(frameSubscriber, inlineExecutor());
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
  EXPECT_CALL(*wireSubscription, cancel_()).Times(1);

  frameSubscriber->subscription()->cancel();
  framedReader->onComplete();
}

TEST(FramedReaderTest, InvalidDataStream) {
  auto rsConnection = std::make_unique<InlineConnection>();
  auto testConnection = std::make_unique<InlineConnection>();

  rsConnection->connectTo(*testConnection);

  auto framedRsAutomatonConnection = std::make_unique<FramedDuplexConnection>(
      std::move(rsConnection), inlineExecutor());

  // Dump 1 invalid frame and expect an error
  auto inputSubscription = std::make_shared<MockSubscription>();

  EXPECT_CALL(*inputSubscription, request_(_)).WillOnce(Invoke([&](size_t n) {
    auto invalidFrameSizePayload =
        folly::IOBuf::createCombined(sizeof(int32_t));
    folly::io::Appender appender(
        invalidFrameSizePayload.get(), /* do not grow */ 0);
    appender.writeBE<int32_t>(1);

    testConnection->getOutput()->onNext(std::move(invalidFrameSizePayload));
  }));
  EXPECT_CALL(*inputSubscription, cancel_()).WillOnce(Invoke([&]() {
    testConnection->getOutput()->onComplete();
  }));

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));
  EXPECT_CALL(*testOutputSubscriber, onNext_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
        // SETUP frame with leading frame size
      }));

  EXPECT_CALL(*testOutputSubscriber, onComplete_()).Times(0);
  EXPECT_CALL(*testOutputSubscriber, onError_(_)).Times(1);

  testConnection->setInput(testOutputSubscriber);
  testConnection->getOutput()->onSubscribe(inputSubscription);

  auto reactiveSocket = StandardReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(framedRsAutomatonConnection),
      // No interactions on this mock, the client will not accept any
      // requests.
      std::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload("test client payload")));
}
