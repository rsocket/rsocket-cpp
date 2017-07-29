// Copyright 2004-present Facebook. All Rights Reserved.

#include <thread>

#include "RSocketTests.h"

#include "test/handlers/HelloServiceHandler.h"

#include "yarpl/flowable/TestSubscriber.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;
using namespace yarpl::flowable;

TEST(WarmResumptionTest, SimpleStream) {
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
