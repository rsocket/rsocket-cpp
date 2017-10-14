// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>
#include "yarpl/flowable/TestSubscriber.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;
using namespace yarpl::flowable;

TEST(RSocketClient, ConnectFails) {
  folly::ScopedEventBaseThread worker;

  folly::SocketAddress address;
  address.setFromHostPort("localhost", 1);
  auto client = RSocket::createConnectedClient(
      std::make_unique<TcpConnectionFactory>(*worker.getEventBase(),
                                             std::move(address)));

  client.then([&](auto&) {
    FAIL() << "the test needs to fail";
  }).onError([&](const std::exception&) {
    LOG(INFO) << "connection failed as expected";
  }).get();
}

TEST(RSocketClientServer, LimitActiveStreamCount) {
  folly::ScopedEventBaseThread worker;
  auto server = makeServer(nullptr);
  auto client = makeClient(worker.getEventBase(), *server->listeningPort());
  client->setMaxActiveStreams(0u);

  auto requester = client->getRequester();
  auto ts = TestSubscriber<std::string>::create();
  requester
      ->requestChannel(
          Flowables::justN({"/hello", "Bob", "Jane"})->map([](std::string v) {
            return Payload(v);
          }))
      ->map([](auto p) { return p.moveDataToString(); })
      ->subscribe(ts);

  ts->awaitTerminalEvent();
  ts->assertOnErrorMessage("it's not possible to create a new stream now");
}