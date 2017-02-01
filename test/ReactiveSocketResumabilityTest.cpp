// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>

#include "src/NullRequestHandler.h"
#include "src/StandardReactiveSocket.h"
#include "test/InlineConnection.h"
#include "test/MockStats.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(ReactiveSocketResumabilityTest, Disconnect) {
  auto socketConnection = std::make_unique<InlineConnection>();
  auto testConnection = std::make_unique<InlineConnection>();

  socketConnection->connectTo(*testConnection);

  auto testInputSubscription = std::make_shared<MockSubscription>();

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));

  testConnection->setInput(testOutputSubscriber);
  testConnection->getOutput()->onSubscribe(testInputSubscription);

  MockStats stats;

  auto socket = StandardReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(socketConnection),
      std::make_unique<DefaultRequestHandler>(),
      ConnectionSetupPayload(),
      stats);

  auto responseSubscriber = std::make_shared<MockSubscriber<Payload>>();
  EXPECT_CALL(*responseSubscriber, onSubscribe_(_))
      .Times(1)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        subscription->request(std::numeric_limits<size_t>::max());
      }));

  EXPECT_CALL(*responseSubscriber, onComplete_()).Times(0);
  EXPECT_CALL(*responseSubscriber, onError_(_)).Times(0);

  socket->requestResponse(Payload(), responseSubscriber);

  EXPECT_CALL(*testOutputSubscriber, onComplete_()).Times(1);
  EXPECT_CALL(*testOutputSubscriber, onError_(_)).Times(0);
  EXPECT_CALL(*testInputSubscription, cancel_()).Times(1);
  EXPECT_CALL(stats, socketDisconnected()).Times(1);
  EXPECT_CALL(stats, socketClosed(_)).Times(0);

  socket->disconnect();

  Mock::VerifyAndClearExpectations(responseSubscriber.get());
  Mock::VerifyAndClearExpectations(testOutputSubscriber.get());
  Mock::VerifyAndClearExpectations(testInputSubscription.get());
  Mock::VerifyAndClearExpectations(&stats);

  EXPECT_CALL(*responseSubscriber, onError_(_)).Times(1);
  EXPECT_CALL(stats, socketDisconnected()).Times(0);
  EXPECT_CALL(stats, socketClosed(_)).Times(1);

  socket->close();
  socket.reset();
  testConnection->getOutput()->onComplete();
}
