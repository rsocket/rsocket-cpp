// Copyright 2004-present Facebook. All Rights Reserved.

#include <thread>

#include "RSocketTests.h"

#include "test/handlers/HelloServiceHandler.h"
#include "test/handlers/HelloStreamRequestHandler.h"

#include "yarpl/flowable/TestSubscriber.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;
using namespace yarpl::flowable;

TEST(WarmResumptionTest, SuccessfulResumption) {
  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());
  auto client = makeResumableClient(*server->listeningPort());
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(7 /* initialRequestN */);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  // Wait for a few frames before disconnecting.
  while (ts->getValueCount() < 3) {
    std::this_thread::yield();
  }
  client->disconnect(std::runtime_error("Test triggered disconnect"));
  EXPECT_NO_THROW(client->resume().get());
  ts->request(3);
  ts->awaitTerminalEvent();
  ts->assertSuccess();
  ts->assertValueCount(10);
}

// Verify after resumption the client is able to consume stream
// from within onError() context
TEST(WarmResumptionTest, FailedResumption1) {
  auto server =
      makeServer(std::make_shared<rsocket::tests::HelloStreamRequestHandler>());
  auto listeningPort = *server->listeningPort();
  auto client = makeResumableClient(listeningPort);
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(7 /* initialRequestN */);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  // Wait for a few frames before disconnecting.
  while (ts->getValueCount() < 3) {
    std::this_thread::yield();
  }
  client->disconnect(std::runtime_error("Test triggered disconnect"));
  client->resume()
      .then([] { FAIL() << "Resumption succeeded when it should not"; })
      .onError([listeningPort](folly::exception_wrapper ex) {
        LOG(INFO) << ex.what();
        auto newClient = makeResumableClient(listeningPort);
        auto newTs =
            TestSubscriber<std::string>::create(6 /* initialRequestN */);
        newClient->getRequester()
            ->requestStream(Payload("Alice"))
            ->map([](auto p) { return p.moveDataToString(); })
            ->subscribe(newTs);
        while (newTs->getValueCount() < 3) {
          std::this_thread::yield();
        }
        newTs->request(2);
        newTs->request(2);
        newTs->awaitTerminalEvent();
        newTs->assertSuccess();
        newTs->assertValueCount(10);
      })
      .wait();
}

// Verify after resumption, the client is able to consume stream
// from within and outside of onError() context
TEST(WarmResumptionTest, FailedResumption2) {
  auto server =
      makeServer(std::make_shared<rsocket::tests::HelloStreamRequestHandler>());
  auto listeningPort = *server->listeningPort();
  auto client = makeResumableClient(listeningPort);
  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create(7 /* initialRequestN */);
  requester->requestStream(Payload("Bob"))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);
  // Wait for a few frames before disconnecting.
  while (ts->getValueCount() < 3) {
    std::this_thread::yield();
  }
  client->disconnect(std::runtime_error("Test triggered disconnect"));
  auto newTs = TestSubscriber<std::string>::create(6 /* initialRequestN */);
  std::shared_ptr<RSocketClient> newClient;
  client->resume()
      .then([] { FAIL() << "Resumption succeeded when it should not"; })
      .onError([listeningPort, newTs, &newClient](folly::exception_wrapper ex) {
        LOG(INFO) << ex.what();
        newClient = makeResumableClient(listeningPort);
        newClient->getRequester()
            ->requestStream(Payload("Alice"))
            ->map([](auto p) { return p.moveDataToString(); })
            ->subscribe(newTs);
        while (newTs->getValueCount() < 3) {
          std::this_thread::yield();
        }
        newTs->request(2);
      })
      .wait();
  newTs->request(2);
  newTs->awaitTerminalEvent();
  newTs->assertSuccess();
  newTs->assertValueCount(10);
}
