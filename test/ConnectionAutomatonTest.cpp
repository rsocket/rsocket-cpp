// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/ConnectionAutomaton.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/framed/FramedWriter.h"
#include "test/InlineConnection.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

TEST(ConnectionAutomatonTest, RefuseFrame) {
  // TODO: the following test is functionally correct and validates the
  //       invalid frame functionality. But with the current memory model
  //       around DuplexConnection, its Subcribers and InlineConnection
  //       it is impossible to make the destruction and cleanup of the test
  //       clean and without use-after-free
  // TODO(lehecka): will fix this in the next iteration, with a different
  //                memory model

  //  auto automatonConnection = folly::make_unique<InlineConnection>();
  //  auto testConnection = folly::make_unique<InlineConnection>();
  //
  //  automatonConnection->connectTo(*testConnection);
  //
  //  auto framedAutomatonConnection =
  //  folly::make_unique<FramedDuplexConnection>(
  //      std::move(automatonConnection));
  //
  //  auto framedTestConnection =
  //      folly::make_unique<FramedDuplexConnection>(std::move(testConnection));
  //
  //  // dump 3 frames to ConnectionAutomaton
  //  // the first frame should be refused and the connection closed
  //  // the last 2 frames should be ignored
  //  // everything should die gracefully
  //
  //  static const int streamId = 1;
  //  UnmanagedMockSubscription inputSubscription;
  //
  //  Sequence s;
  //
  //  EXPECT_CALL(inputSubscription, request_(_))
  //      .InSequence(s)
  //      .WillOnce(Invoke([&](size_t n) {
  //        auto& framedWriter =
  //        dynamic_cast<FramedWriter&>(framedTestConnection->getOutput());
  //
  //        std::vector<std::unique_ptr<folly::IOBuf>> frames;
  //        frames.push_back(Frame_REQUEST_N(streamId, 1).serializeOut());
  //        frames.push_back(Frame_REQUEST_N(streamId + 1, 1).serializeOut());
  //        frames.push_back(Frame_REQUEST_N(streamId + 2, 1).serializeOut());
  //
  //        framedWriter.onNextMultiple(std::move(frames));
  //      }));
  //
  //  UnmanagedMockSubscriber<std::unique_ptr<folly::IOBuf>>
  //  testOutputSubscriber;
  //  EXPECT_CALL(testOutputSubscriber, onSubscribe_(_))
  //      .InSequence(s)
  //      .WillOnce(Invoke([&](Subscription* subscription) {
  //        // allow receiving frames from the automaton
  //        subscription->request(std::numeric_limits<size_t>::max());
  //      }));
  //  //TODO: add subscriber and call request 1 to get onNext
  //  EXPECT_CALL(testOutputSubscriber, onNext_(_))
  //      .InSequence(s)
  //      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
  //        auto frameType = FrameHeader::peekType(*frame);
  //        ASSERT_EQ(FrameType::ERROR, frameType);
  //      }));
  //  EXPECT_CALL(testOutputSubscriber, onComplete_()).Times(1).InSequence(s);
  //
  //  framedTestConnection->setInput(testOutputSubscriber);
  //  framedTestConnection->getOutput().onSubscribe(inputSubscription);
  //
  //  ConnectionAutomaton connectionAutomaton(
  //      std::move(framedAutomatonConnection),
  //      [](StreamId, std::unique_ptr<folly::IOBuf>) { return false; },
  //      nullptr,
  //      Stats::noop(),
  //      false);
  //  connectionAutomaton.connect();
}
