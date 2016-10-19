// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/Memory.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "src/Frame.h"
#include "src/ConnectionAutomaton.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/framed/FramedWriter.h"
#include "test/InlineConnection.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

// namespace {
//
// std::unique_ptr<folly::IOBuf> makeInvalidHeader() {
//   // Create a header without the stream id
//   folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
//   queue.append(folly::IOBuf::create(FrameHeader::kSize - sizeof(StreamId)));
//   folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
//   appender.writeBE<uint16_t>(static_cast<uint16_t>(FrameType::REQUEST_N));
//   appender.writeBE<uint16_t>(FrameFlags_EMPTY);
//   return queue.move();
// }
//
// }

// TODO: the following tests are functionally correct and validates the
//       invalid frame functionality. But with the current memory model
//       around DuplexConnection, its Subcribers and InlineConnection
//       it is impossible to make the destruction and cleanup of the test
//       clean and without use-after-free
// Both tests in this file use InlineConnection which will be destructed
// inside InlineConnection::setInput() when it calls inputSink.onSubscribe().
// TODO(lehecka): will fix this in the next iteration, with a different
//                memory model

TEST(ConnectionAutomatonTest, InvalidFrameHeader) {
  // auto automatonConnection = folly::make_unique<InlineConnection>();
  // auto testConnection = folly::make_unique<InlineConnection>();
  //
  // automatonConnection->connectTo(*testConnection);
  //
  // auto framedAutomatonConnection = folly::make_unique<FramedDuplexConnection>(
  //     std::move(automatonConnection));
  //
  // auto framedTestConnection =
  //     folly::make_unique<FramedDuplexConnection>(std::move(testConnection));
  //
  // // Dump 1 invalid frame and expect an error
  //
  // UnmanagedMockSubscription inputSubscription;
  //
  // Sequence s;
  //
  // EXPECT_CALL(inputSubscription, request_(_))
  //     .InSequence(s)
  //     .WillOnce(Invoke([&](size_t n) {
  //       auto& framedWriter =
  //           dynamic_cast<FramedWriter&>(framedTestConnection->getOutput());
  //
  //       std::vector<std::unique_ptr<folly::IOBuf>> frames;
  //       frames.push_back(makeInvalidHeader());
  //       framedWriter.onNextMultiple(std::move(frames));
  //     }));
  //
  // UnmanagedMockSubscriber<std::unique_ptr<folly::IOBuf>> testOutputSubscriber;
  // EXPECT_CALL(testOutputSubscriber, onSubscribe_(_))
  //     .WillOnce(Invoke([&](Subscription* subscription) {
  //       // allow receiving frames from the automaton
  //       subscription->request(std::numeric_limits<size_t>::max());
  //     }));
  // EXPECT_CALL(testOutputSubscriber, onNext_(_))
  //     .InSequence(s)
  //     .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
  //       auto frameType = FrameHeader::peekType(*frame);
  //       Frame_ERROR error;
  //       ASSERT_EQ(FrameType::ERROR, frameType);
  //       ASSERT_TRUE(error.deserializeFrom(std::move(frame)));
  //       ASSERT_EQ("invalid frame", error.payload_.moveDataToString());
  //     }));
  // EXPECT_CALL(testOutputSubscriber, onComplete_()).Times(1).InSequence(s);
  // EXPECT_CALL(inputSubscription, cancel_()).Times(1).InSequence(s);
  //
  // framedTestConnection->setInput(testOutputSubscriber);
  // framedTestConnection->getOutput().onSubscribe(inputSubscription);
  //
  // ConnectionAutomaton connectionAutomaton(
  //     std::move(framedAutomatonConnection),
  //     [](StreamId, std::unique_ptr<folly::IOBuf>) { return false; },
  //     nullptr,
  //     Stats::noop(),
  //     false);
  // connectionAutomaton.connect();
}

TEST(ConnectionAutomatonTest, RefuseFrame) {
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
