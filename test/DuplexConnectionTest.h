// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <gtest/gtest.h>
#include <src/ConnectionAcceptor.h>
#include <src/ConnectionFactory.h>
#include <src/framing/FrameSerializer.h>
#include "test/test_utils/BasicClientServer.h"
#include "src/framing/FrameTransport.h"
#include <src/framing/FrameSerializer_v1_0.h>

using namespace ::testing;

namespace rsocket {
namespace tests {
namespace duplexconnection {

class TestSubscriber : public MockSubscriber<std::unique_ptr<folly::IOBuf>> {
public:
  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    MockSubscriber::onSubscribe(subscription);
    subscription->request(100);
  }
};

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

  connectedClient->async([](DuplexConnection* connection) {
    auto clientSubscriber = yarpl::make_ref<TestSubscriber>();
    EXPECT_CALL(*clientSubscriber, onSubscribe_(_));
    EXPECT_CALL(*clientSubscriber, onNext_(_));
    EXPECT_CALL(*clientSubscriber, onComplete_());
    connection->setInput(clientSubscriber);

    Frame_KEEPALIVE pingFrame;
    auto frameSerializer = FrameSerializer::createCurrentVersion();
    auto output = connection->getOutput();
    auto subscription = yarpl::make_ref<MockSubscription>();
    EXPECT_CALL(*subscription, cancel_());
    EXPECT_CALL(*subscription, request_(std::numeric_limits<int32_t>::max()));
    output->onSubscribe(subscription);

    VLOG(3) << "Sending PingFrame: " << pingFrame;
    output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
    output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
    output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));
    output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));

    output->onComplete();
    subscription->cancel();
  });

  connectedServer->async([](DuplexConnection* connection) {
    auto serverSubscriber = yarpl::make_ref<TestSubscriber>();
    EXPECT_CALL(*serverSubscriber, onSubscribe_(_));
    EXPECT_CALL(*serverSubscriber, onNext_(_)).Times(AtLeast(1));
    EXPECT_CALL(*serverSubscriber, onComplete_());
    connection->setInput(serverSubscriber);
  }).wait();

  connectedServer->async([](DuplexConnection* connection) {
    auto output = connection->getOutput();
    auto subscription = yarpl::make_ref<MockSubscription>();
    EXPECT_CALL(*subscription, cancel_());
    EXPECT_CALL(*subscription, request_(std::numeric_limits<int32_t>::max()));
    output->onSubscribe(subscription);

    auto frameSerializer = FrameSerializer::createCurrentVersion();
    Frame_KEEPALIVE pingFrame(FrameFlags::KEEPALIVE_RESPOND, 101, folly::IOBuf::copyBuffer("424242"));
    output->onNext(frameSerializer->serializeOut(std::move(pingFrame), false));

    output->onComplete();
    subscription->cancel();

    VLOG(3) << "Executed at ConnectedServer";
  });

  // Get char to quit
  std::getchar();
}

} // namespace duplexconnection
} // namespace tests
} // namespace rsocket