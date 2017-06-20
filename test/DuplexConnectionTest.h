// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gtest/gtest.h>
#include <src/ConnectionAcceptor.h>
#include <src/ConnectionFactory.h>
#include <src/framing/FrameSerializer.h>
#include "test/test_utils/BasicClientServer.h"
#include "src/framing/FrameTransport.h"
#include <src/framing/FrameSerializer_v1_0.h>

namespace rsocket {
namespace tests {
namespace duplexconnection {

template <
    typename ConnectionAcceptor,
    typename ConnectionFactory,
    typename = std::enable_if_t<std::is_base_of<
        rsocket::ConnectionFactory,
        std::decay_t<ConnectionFactory>>::value>,
    typename = std::enable_if_t<std::is_base_of<
        rsocket::ConnectionAcceptor,
        std::decay_t<ConnectionAcceptor>>::value>>
void MultipleSetInputOutputCallsTest() {
  typename ConnectionAcceptor::Options options;
  options.port = 9898;
  Server<ConnectionAcceptor> server(options);
  auto guard = makeGuard([&server]() { server.stop(); });

  // Make a new connection to the server
  Client<ConnectionFactory> client(options.port);
  auto&& connectedClient = client.connect();
  auto&& connectedServer = server.getConnected();

  connectedClient->sendSetupFrame();
  connectedServer->receiveSetupFrame();

  for (int i = 0; i < 10; ++i) {
    connectedClient->async([](DuplexConnection *connection) {
      auto subscriber = yarpl::make_ref<TestSubscriber>();
      EXPECT_CALL(*subscriber, onSubscribe_(_));
      EXPECT_CALL(*subscriber, onNext_(_)).Times(Exactly(4));
      EXPECT_CALL(*subscriber, onComplete_());
      connection->setInput(subscriber);

      Frame_KEEPALIVE pingFrame;
      auto frameSerializer = FrameSerializer::createCurrentVersion();
      auto output = connection->getOutput();
      auto subscription = yarpl::make_ref<MockSubscription>();
      EXPECT_CALL(*subscription, cancel_());
      EXPECT_CALL(*subscription, request_(std::numeric_limits<int32_t>::max()));
      output->onSubscribe(subscription);

      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));

      output->onComplete();
      subscription->cancel();
    }).wait();

    connectedServer->async([](DuplexConnection *connection) {
      auto subscriber = yarpl::make_ref<TestSubscriber>();
      EXPECT_CALL(*subscriber, onSubscribe_(_));
      EXPECT_CALL(*subscriber, onNext_(_)).Times(Exactly(4));
      EXPECT_CALL(*subscriber, onComplete_());
      connection->setInput(subscriber);

      auto output = connection->getOutput();
      auto subscription = yarpl::make_ref<MockSubscription>();
      EXPECT_CALL(*subscription, cancel_());
      EXPECT_CALL(*subscription, request_(std::numeric_limits<int32_t>::max()));
      output->onSubscribe(subscription);

      auto frameSerializer = FrameSerializer::createCurrentVersion();
      Frame_KEEPALIVE pingFrame(FrameFlags::KEEPALIVE_RESPOND, 101, folly::IOBuf::copyBuffer("424242"));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
      output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));

      output->onComplete();
      subscription->cancel();
      connection->setInput(nullptr); // don't continue receiving
    }).wait();
  }
}

} // namespace duplexconnection
} // namespace tests
} // namespace rsocket