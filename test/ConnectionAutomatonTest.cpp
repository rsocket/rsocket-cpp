// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>

#include <folly/Memory.h>
#include <folly/io/Cursor.h>
#include <gmock/gmock.h>
#include <src/FrameSerializer.h>
#include <src/NullRequestHandler.h>
#include "src/ConnectionAutomaton.h"
#include "src/FrameTransport.h"
#include "src/StreamState.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/framed/FramedWriter.h"
#include "test/InlineConnection.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

static std::unique_ptr<folly::IOBuf> makeInvalidFrameHeader() {
  // Create a header without the stream id
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());

  queue.append(folly::IOBuf::create(sizeof(uint16_t)));

  folly::io::QueueAppender appender(&queue, /* do not grow */ 0);
  appender.writeBE<uint16_t>(static_cast<uint16_t>(FrameType::REQUEST_N));
  return queue.move();
}

TEST(ConnectionAutomatonTest, InvalidFrameHeader) {
  auto automatonConnection = std::make_unique<InlineConnection>();
  auto testConnection = std::make_unique<InlineConnection>();

  automatonConnection->connectTo(*testConnection);

  auto framedAutomatonConnection = std::make_unique<FramedDuplexConnection>(
      std::move(automatonConnection), inlineExecutor());

  auto framedTestConnection = std::make_unique<FramedDuplexConnection>(
      std::move(testConnection), inlineExecutor());

  // Dump 1 invalid frame and expect an error

  auto inputSubscription = std::make_shared<MockSubscription>();

  EXPECT_CALL(*inputSubscription, request_(_))
      .Times(AtMost(2))
      .WillOnce(Invoke([&](size_t n) {
        framedTestConnection->getOutput()->onNext(makeInvalidFrameHeader());
      }))
      .WillOnce(
          /*this call is because of async scheduling on executor*/ Return());

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));
  EXPECT_CALL(*testOutputSubscriber, onNext_(_))
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
        auto frameSerializer = FrameSerializer::createCurrentVersion();
        auto frameType = frameSerializer->peekFrameType(*frame);
        Frame_ERROR error;
        ASSERT_EQ(FrameType::ERROR, frameType);
        bool deserialized =
            frameSerializer->deserializeFrom(error, std::move(frame));
        ASSERT_TRUE(deserialized);
        ASSERT_EQ("invalid frame", error.payload_.moveDataToString());
      }));
  EXPECT_CALL(*testOutputSubscriber, onComplete_()).Times(1);
  EXPECT_CALL(*testOutputSubscriber, onError_(_)).Times(0);

  framedTestConnection->setInput(testOutputSubscriber);
  framedTestConnection->getOutput()->onSubscribe(inputSubscription);

  std::shared_ptr<ConnectionAutomaton> connectionAutomaton;
  connectionAutomaton = std::make_shared<ConnectionAutomaton>(
      defaultExecutor(),
      nullptr,
      std::make_shared<NullRequestHandler>(),
      Stats::noop(),
      nullptr,
      ReactiveSocketMode::CLIENT);
  connectionAutomaton->setFrameSerializer(
      FrameSerializer::createCurrentVersion());
  connectionAutomaton->connect(
      std::make_shared<FrameTransport>(std::move(framedAutomatonConnection)),
      true);
  connectionAutomaton->close(
      folly::exception_wrapper(), StreamCompletionSignal::CONNECTION_END);
}

static void terminateTest(
    bool inOnSubscribe,
    bool inOnNext,
    bool inOnComplete,
    bool inRequest) {
  auto automatonConnection = std::make_unique<InlineConnection>();
  auto testConnection = std::make_unique<InlineConnection>();

  automatonConnection->connectTo(*testConnection);

  auto framedAutomatonConnection = std::make_unique<FramedDuplexConnection>(
      std::move(automatonConnection), inlineExecutor());

  auto framedTestConnection = std::make_unique<FramedDuplexConnection>(
      std::move(testConnection), inlineExecutor());

  auto inputSubscription = std::make_shared<MockSubscription>();

  if (!inOnSubscribe) {
    auto&& expexctation =
        EXPECT_CALL(*inputSubscription, request_(_))
            .Times(AtMost(2))
            .WillOnce(Invoke([&](size_t n) {
              if (inRequest) {
                framedTestConnection->getOutput()->onComplete();
              } else {
                framedTestConnection->getOutput()->onNext(
                    makeInvalidFrameHeader());
              }
            }));

    if (!inRequest) {
      expexctation.WillOnce(
          /*this call is because of async scheduling on executor*/ Return());
    }
  }

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        if (inOnSubscribe) {
          subscription->cancel();
        } else {
          // allow receiving frames from the automaton
          subscription->request(std::numeric_limits<size_t>::max());
        }
      }));
  if (!inOnSubscribe && !inRequest) {
    EXPECT_CALL(*testOutputSubscriber, onNext_(_))
        .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
          if (inOnNext) {
            testOutputSubscriber->subscription()->cancel();
          }
        }));
  }
  EXPECT_CALL(*testOutputSubscriber, onComplete_()).WillOnce(Invoke([&]() {
    if (inOnComplete) {
      testOutputSubscriber->subscription()->cancel();
    }
  }));

  auto testOutput = framedTestConnection->getOutput();

  framedTestConnection->setInput(testOutputSubscriber);
  framedTestConnection->getOutput()->onSubscribe(inputSubscription);

  std::shared_ptr<ConnectionAutomaton> connectionAutomaton;
  connectionAutomaton = std::make_shared<ConnectionAutomaton>(
      defaultExecutor(),
      nullptr,
      std::make_shared<NullRequestHandler>(),
      Stats::noop(),
      nullptr,
      ReactiveSocketMode::CLIENT);
  connectionAutomaton->setFrameSerializer(
      FrameSerializer::createCurrentVersion());
  connectionAutomaton->connect(
      std::make_shared<FrameTransport>(std::move(framedAutomatonConnection)),
      true);
  connectionAutomaton->close(
      folly::exception_wrapper(), StreamCompletionSignal::CONNECTION_END);
}

TEST(ConnectionAutomatonTest, CleanTerminateOnSubscribe) {
  terminateTest(true, false, false, false);
}

TEST(ConnectionAutomatonTest, CleanTerminateOnNext) {
  terminateTest(false, true, false, false);
}

TEST(ConnectionAutomatonTest, CleanTerminateOnComplete) {
  terminateTest(true, true, true, false);
}

TEST(ConnectionAutomatonTest, CleanTerminateRequest) {
  terminateTest(false, true, false, true);
}

TEST(ConnectionAutomatonTest, RefuseFrame) {
  auto automatonConnection = std::make_unique<InlineConnection>();
  auto testConnection = std::make_unique<InlineConnection>();

  automatonConnection->connectTo(*testConnection);

  auto framedAutomatonConnection = std::make_unique<FramedDuplexConnection>(
      std::move(automatonConnection), inlineExecutor());

  auto framedTestConnection = std::make_unique<FramedDuplexConnection>(
      std::move(testConnection), inlineExecutor());

  // dump 3 frames to ConnectionAutomaton
  // the first frame should be refused and the connection closed
  // the last 2 frames should be ignored
  // everything should die gracefully

  static const int streamId = 1;
  auto inputSubscription = std::make_shared<MockSubscription>();

  Sequence s;

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));

  EXPECT_CALL(*inputSubscription, request_(_))
      .Times(AtMost(2))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t n) {
        auto framedWriter = std::dynamic_pointer_cast<FramedWriter>(
            framedTestConnection->getOutput());
        CHECK(framedWriter);
        auto frameSerializer = FrameSerializer::createCurrentVersion();
        std::vector<std::unique_ptr<folly::IOBuf>> frames;
        frames.push_back(
            frameSerializer->serializeOut(Frame_REQUEST_N(streamId, 1)));
        frames.push_back(
            frameSerializer->serializeOut(Frame_REQUEST_N(streamId + 1, 1)));
        frames.push_back(
            frameSerializer->serializeOut(Frame_REQUEST_N(streamId + 2, 1)));

        framedWriter->onNextMultiple(std::move(frames));
      }))
      .WillOnce(
          /*this call is because of async scheduling on executor*/ Return());
  EXPECT_CALL(*testOutputSubscriber, onNext_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
        auto frameType =
            FrameSerializer::createCurrentVersion()->peekFrameType(*frame);
        ASSERT_EQ(FrameType::ERROR, frameType);
      }));
  EXPECT_CALL(*testOutputSubscriber, onComplete_()).Times(1).InSequence(s);

  framedTestConnection->setInput(testOutputSubscriber);
  framedTestConnection->getOutput()->onSubscribe(inputSubscription);

  std::shared_ptr<ConnectionAutomaton> connectionAutomaton;
  connectionAutomaton = std::make_shared<ConnectionAutomaton>(
      defaultExecutor(),
      nullptr,
      std::make_shared<NullRequestHandler>(),
      Stats::noop(),
      nullptr,
      ReactiveSocketMode::CLIENT);
  connectionAutomaton->setFrameSerializer(
      FrameSerializer::createCurrentVersion());
  connectionAutomaton->connect(
      std::make_shared<FrameTransport>(std::move(framedAutomatonConnection)),
      true);
  connectionAutomaton->close(
      folly::exception_wrapper(), StreamCompletionSignal::CONNECTION_END);
}
