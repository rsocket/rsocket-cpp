// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>
#include <thread>

#include "RSocketTests.h"

#include "rsocket/test/handlers/HelloServiceHandler.h"

#include "yarpl/flowable/TestSubscriber.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace testing;
using namespace rsocket::tests::client_server;
using namespace yarpl::flowable;

namespace {

class MockConnEvents : public RSocketConnectionEvents {
 public:
  MOCK_METHOD0(onConnected, void());
  MOCK_METHOD1(onDisconnected, void(const folly::exception_wrapper&));
  MOCK_METHOD0(onStreamsPaused, void());
  MOCK_METHOD0(onStreamsResumed, void());
  MOCK_METHOD1(onClosed, void(const folly::exception_wrapper&));
};

} // anonymous namespace

TEST(ConnectionEventsTest, SimpleStream) {
  folly::ScopedEventBaseThread worker;
  auto serverConnEvents = std::make_shared<StrictMock<MockConnEvents>>();
  auto clientConnEvents = std::make_shared<StrictMock<MockConnEvents>>();

  EXPECT_CALL(*clientConnEvents, onConnected());
  EXPECT_CALL(*serverConnEvents, onConnected());

  // create server supporting resumption
  auto server = makeResumableServer(
      std::make_shared<HelloServiceHandler>(serverConnEvents));

  // create resumable client
  auto client = makeWarmResumableClient(
      worker.getEventBase(), *server->listeningPort(), clientConnEvents);

  // request stream
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(7 /* initialRequestN */);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  // Wait for a few frames before disconnecting.
  while (ts->getValueCount() < 3) {
    std::this_thread::yield();
  }

  // disconnect
  EXPECT_CALL(*clientConnEvents, onDisconnected(_));
  EXPECT_CALL(*clientConnEvents, onStreamsPaused());
  EXPECT_CALL(*serverConnEvents, onDisconnected(_));
  EXPECT_CALL(*serverConnEvents, onStreamsPaused());
  client->disconnect(std::runtime_error("Test triggered disconnect"));

  // resume
  EXPECT_CALL(*clientConnEvents, onConnected());
  EXPECT_CALL(*clientConnEvents, onStreamsResumed());
  EXPECT_CALL(*serverConnEvents, onConnected());
  EXPECT_CALL(*serverConnEvents, onStreamsResumed());
  EXPECT_NO_THROW(client->resume().get());

  ts->request(3);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);

  // disconnect
  EXPECT_CALL(*clientConnEvents, onDisconnected(_));
  EXPECT_CALL(*clientConnEvents, onStreamsPaused());
  EXPECT_CALL(*serverConnEvents, onDisconnected(_));
  EXPECT_CALL(*serverConnEvents, onStreamsPaused());
  client->disconnect(std::runtime_error("Test triggered disconnect"));

  // relinquish resources
  EXPECT_CALL(*clientConnEvents, onClosed(_));
  EXPECT_CALL(*serverConnEvents, onClosed(_));
}

// Verify the ConnectionEvents are called back on the right EventBase.
TEST(ConnectionEventsTest, DifferentEvb) {
  folly::ScopedEventBaseThread transportWorker{"TransportWkr"};
  folly::ScopedEventBaseThread SMWorker{"SMWorker"};

  auto clientConnEvents = std::make_shared<StrictMock<MockConnEvents>>();

  EXPECT_CALL(*clientConnEvents, onConnected())
      .WillOnce(Invoke([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));

  // create server supporting resumption
  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());

  // create resumable client
  auto client = makeWarmResumableClient(
      transportWorker.getEventBase(),
      *server->listeningPort(),
      clientConnEvents,
      SMWorker.getEventBase());

  // request stream
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(7 /* initialRequestN */);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  // Wait for a few frames before disconnecting.
  while (ts->getValueCount() < 3) {
    std::this_thread::yield();
  }

  // disconnect
  EXPECT_CALL(*clientConnEvents, onDisconnected(_))
      .WillOnce(InvokeWithoutArgs([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
  EXPECT_CALL(*clientConnEvents, onStreamsPaused())
      .WillOnce(Invoke([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
  client->disconnect(std::runtime_error("Test triggered disconnect"));

  // resume
  EXPECT_CALL(*clientConnEvents, onConnected())
      .WillOnce(Invoke([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
  EXPECT_CALL(*clientConnEvents, onStreamsResumed())
      .WillOnce(Invoke([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
  EXPECT_NO_THROW(client->resume().get());

  ts->request(3);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);

  // disconnect
  EXPECT_CALL(*clientConnEvents, onDisconnected(_))
      .WillOnce(InvokeWithoutArgs([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
  EXPECT_CALL(*clientConnEvents, onStreamsPaused())
      .WillOnce(Invoke([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
  client->disconnect(std::runtime_error("Test triggered disconnect"));

  // relinquish resources
  EXPECT_CALL(*clientConnEvents, onClosed(_))
      .WillOnce(InvokeWithoutArgs([evb = SMWorker.getEventBase()]() {
        EXPECT_TRUE(evb->isInEventBaseThread());
      }));
}
