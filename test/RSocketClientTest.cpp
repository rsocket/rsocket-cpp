// Copyright 2004-present Facebook. All Rights Reserved.

#include "RSocketTests.h"

#include <folly/io/async/ScopedEventBaseThread.h>
#include <gtest/gtest.h>

#include "rsocket/transports/tcp/TcpConnectionFactory.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;

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
