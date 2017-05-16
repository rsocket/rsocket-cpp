// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>
#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include "test/InlineConnection.h"
#include "test/streams/Mocks.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(InlineConnectionTest, PingPong) {
  // InlineConnection forward appropriate calls in-line, hence the order of mock
  // calls will be deterministic.
  Sequence s;
  std::array<InlineConnection, 2> end;
  end[0].connectTo(end[1]);

  std::array<std::shared_ptr<MockSubscriber<std::unique_ptr<folly::IOBuf>>>, 2>
      input;
  input[0] = std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  input[1] = std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  std::array<std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>>, 2>
      output;

  std::array<std::shared_ptr<MockSubscription>, 2> outputSub;
  outputSub[0] = std::make_shared<MockSubscription>();
  outputSub[1] = std::make_shared<MockSubscription>();
  std::array<std::shared_ptr<Subscription>, 2> inputSub;

  for (size_t i = 0; i < 2; ++i) {
    EXPECT_CALL(*input[i], onSubscribe_(_))
        .InSequence(s)
        .WillRepeatedly(Invoke([&inputSub, i](
            std::shared_ptr<Subscription> sub) { inputSub[i] = sub; }));
  }

  // Register inputs and outputs in two different orders for two different
  // "directions" of the connection.
  end[0].setInput(input[0]);
  output[1] = end[1].getOutput();
  output[1]->onSubscribe(outputSub[1]);
  output[0] = end[0].getOutput();
  output[0]->onSubscribe(outputSub[0]);
  end[1].setInput(input[1]);

  auto originalPayload = folly::IOBuf::copyBuffer("request1");
  EXPECT_CALL(*outputSub[1], request_(1)).InSequence(s);
  EXPECT_CALL(*outputSub[0], request_(1))
      .InSequence(s)
      .WillOnce(
          Invoke([&](size_t) { output[0]->onNext(originalPayload->clone()); }));
  EXPECT_CALL(*input[1], onNext_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& payload) {
        ASSERT_TRUE(folly::IOBufEqual()(originalPayload, payload));
        // We know Subscription::request(1) has been called on the corresponding
        // subscription.
        output[1]->onNext(std::move(payload));
      }));
  EXPECT_CALL(*input[0], onNext_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& payload) {
        // We know Subscription::request(1) has been called on the corresponding
        // subscription.
        ASSERT_TRUE(folly::IOBufEqual()(originalPayload, payload));
      }));

  EXPECT_CALL(*outputSub[1], cancel_()).InSequence(s).WillOnce(Invoke([&]() {
    output[1]
        ->onComplete(); // "Unsubscribe handshake". Calls input[0]->onComplete()
    inputSub[1]->cancel(); // Close the other direction. // equivalent to
    // outputSub[0]->cancel();
  }));
  EXPECT_CALL(*input[0], onComplete_())
      .InSequence(s); // This finishes the handshake.
  EXPECT_CALL(*outputSub[0], cancel_()).InSequence(s).WillOnce(Invoke([&]() {
    output[0]
        ->onComplete(); // "Unsubscribe handshake". Calls input[1]->onComplete()
  }));
  EXPECT_CALL(*input[1], onComplete_())
      .InSequence(s); // This finishes the handshake.

  // Make sure flow control allows us to send response back to this end.
  inputSub[0]->request(1);
  // Perform the ping
  inputSub[1]->request(1);
  // Let's shut everything down from the end that requested the ping.
  inputSub[0]->cancel(); // equivalent to outputSub[1]->cancel()
}
