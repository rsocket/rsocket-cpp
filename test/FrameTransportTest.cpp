// Copyright 2004-present Facebook. All Rights Reserved.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <folly/Memory.h>

#include "src/StandardReactiveSocket.h"
#include "src/FrameProcessor.h"
#include "src/FrameTransport.h"

#include "test/streams/Mocks.h"
#include "test/InlineConnection.h"
#include "test/MockRequestHandler.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

namespace {

class StubDuplexConnection : public DuplexConnection {
public:
  StubDuplexConnection()
  : outputSubscriber_(std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>()) {}

  void setInput(std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> framesSink) override {
    // We don't care about this
    framesSink->onSubscribe(std::make_shared<MockSubscription>());
  };

  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> getOutput() override {
    return outputSubscriber_;
  }

  std::shared_ptr<MockSubscriber<std::unique_ptr<folly::IOBuf>>> outputSubscriber_;
};

class MockFrameProcessor : public FrameProcessor {
public:
  MOCK_METHOD0(onReady, void());

  void processFrame(std::unique_ptr<folly::IOBuf> buf) {
    processFrame(buf.get());
  }

  MOCK_METHOD1(processFrame, void(folly::IOBuf*));
  MOCK_METHOD2(onTerminal, void(folly::exception_wrapper, StreamCompletionSignal));
};

}

TEST(FrameTransportTest, NoOnReady) {
  auto duplexConnection = folly::make_unique<StubDuplexConnection>();

  auto frameTransport = std::make_shared<FrameTransport>(std::move(duplexConnection));
  auto frameProcessor = std::make_shared<StrictMock<MockFrameProcessor>>();

  frameTransport->setFrameProcessor(frameProcessor);

  EXPECT_CALL(*frameProcessor, onReady())
    .Times(0);

  frameTransport->close(folly::exception_wrapper());
}

TEST(FrameTransportTest, OnReady) {
  auto duplexConnection = folly::make_unique<StubDuplexConnection>();
  auto outputSubscription = duplexConnection->outputSubscriber_;

  auto frameTransport = std::make_shared<FrameTransport>(std::move(duplexConnection));
  auto frameProcessor = std::make_shared<StrictMock<MockFrameProcessor>>();

  frameTransport->setFrameProcessor(frameProcessor);

  EXPECT_CALL(*frameProcessor, onReady())
    .Times(1);

  // Simulate requesting output
  outputSubscription->subscription()->request(1);
  outputSubscription->subscription()->request(1);

  frameTransport->close(folly::exception_wrapper());
}
