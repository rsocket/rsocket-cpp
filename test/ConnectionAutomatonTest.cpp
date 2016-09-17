// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/ConnectionAutomaton.h"
#include "src/framed/FramedDuplexConnection.h"
#include "test/InlineConnection.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(ConnectionAutomatonTest, RefuseFrame) {
  auto automatonConnection = folly::make_unique<InlineConnection>();
  auto testConnection = folly::make_unique<InlineConnection>();

  automatonConnection->connectTo(*testConnection);

  auto framedAutomatonConnection = folly::make_unique<FramedDuplexConnection>(
      std::move(automatonConnection));

  auto framedTestConnection =
      folly::make_unique<FramedDuplexConnection>(std::move(testConnection));

  // dump 3 frames to ConnectionAutomaton
  // the first frame should be refused and the connection closed
  // the last 2 frames should be ignored
  // everything should die gracefully

  static const int streamId = 1;
  UnmanagedMockSubscription inputSubscription;

  Sequence s;

  EXPECT_CALL(inputSubscription, request_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t n) {
        framedTestConnection->getOutput().onNext(
            Frame_REQUEST_N(streamId, 1).serializeOut());
        framedTestConnection->getOutput().onNext(
            Frame_REQUEST_N(streamId + 1, 1).serializeOut());
        framedTestConnection->getOutput().onNext(
            Frame_REQUEST_N(streamId + 2, 1).serializeOut());
      }));

  UnmanagedMockSubscriber<std::unique_ptr<folly::IOBuf>> testOutputSubscriber;
  EXPECT_CALL(testOutputSubscriber, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](Subscription* subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));
  EXPECT_CALL(testOutputSubscriber, onNext_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
        auto frameType = FrameHeader::peekType(*frame);
        ASSERT_EQ(FrameType::ERROR, frameType);
      }));
  EXPECT_CALL(testOutputSubscriber, onComplete_()).InSequence(s).Times(1);

  framedTestConnection->setInput(testOutputSubscriber);
  framedTestConnection->getOutput().onSubscribe(inputSubscription);

  ConnectionAutomaton connectionAutomaton(
      std::move(framedAutomatonConnection),
      [](StreamId, std::unique_ptr<folly::IOBuf>) { return false; },
      nullptr,
      Stats::noop(),
      false);
  connectionAutomaton.connect();
}
