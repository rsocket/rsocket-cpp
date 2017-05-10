// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/ExceptionWrapper.h>
#include <folly/Format.h>
#include <folly/io/Cursor.h>
#include <gmock/gmock.h>
#include "src/FrameSerializer.h"
#include "src/StandardReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/framed/FramedReader.h"
#include "test/InlineConnection.h"
#include "test/MockRequestHandler.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

class MockDuplexConnection : public DuplexConnection {
 public:
  MOCK_METHOD1(
      setInput,
      void(std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>));
  MOCK_METHOD0(
      getOutput,
      std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>());
};

TEST(FramedDuplexTest, getInputOnActiveDuplex) {
  auto mockDuplexConn = std::make_unique<MockDuplexConnection>();

  auto frameSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();

  auto frameSubscriber2 =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();

  auto wireSubscription = std::make_shared<MockSubscription>();

  std::string part1("val");
  std::string part2("ueXXX");
  std::string msg1 = part1 + part2;
  auto payload = folly::IOBuf::create(0);

  EXPECT_CALL(*mockDuplexConn, setInput(_))
      .WillOnce(Invoke([&](auto framedReader) {
        // Place data into FrameReader
        EXPECT_CALL(*frameSubscriber, onSubscribe_(_)).Times(1);
        framedReader->onSubscribe(wireSubscription);

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
      }))
      .WillOnce(Invoke([&](auto framedReader) {
        // Frame has been changed, finish request
        payload = folly::IOBuf::create(0);
        {
          folly::io::Appender appender(payload.get(), 10);
          folly::format("{}", part2.c_str())(appender);
        }
        // Make sure nothing was lost
        EXPECT_CALL(*frameSubscriber2, onNext_(_))
            .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& p) {
              ASSERT_EQ("valueXXX", p->moveToFbString().toStdString());
            }));

        framedReader->onNext(std::move(payload));
        frameSubscriber2->subscription()->request(1);
      }));

  auto framedDuplexConnection = std::make_unique<FramedDuplexConnection>(
      std::move(mockDuplexConn), inlineExecutor());

  // Set initial frame
  framedDuplexConnection->setInput(frameSubscriber);

  // Swap out frames
  EXPECT_CALL(*frameSubscriber, onComplete_()).Times(1);
  EXPECT_CALL(*frameSubscriber2, onSubscribe_(_)).Times(1);
  framedDuplexConnection->setInput(frameSubscriber2);

  // Clean up
  EXPECT_CALL(*frameSubscriber2, onComplete_()).Times(1);
  EXPECT_CALL(*wireSubscription, cancel_()).Times(1);

  frameSubscriber2->subscription()->cancel();
}
