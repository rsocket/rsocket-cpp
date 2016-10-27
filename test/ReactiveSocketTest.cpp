// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>
#include <chrono>
#include <memory>
#include <thread>

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "MockStats.h"
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "test/InlineConnection.h"
#include "test/MockRequestHandler.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

MATCHER_P(
    Equals,
    payload,
    "Payloads " + std::string(negation ? "don't" : "") + "match") {
  return folly::IOBufEqual()(*payload, arg.data);
};

MATCHER_P(
    Equals2,
    payload,
    "Payloads " + std::string(negation ? "don't" : "") + "match") {
  return folly::IOBufEqual()(*payload, arg);
};

TEST(ReactiveSocketTest, RequestChannel) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub, serverInputSub;
  std::shared_ptr<Subscriber<Payload>> clientOutput, serverOutput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a channel.
  EXPECT_CALL(*clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](std::shared_ptr<Subscription> sub) { clientInputSub = sub; }));
  // The initial payload is requested automatically.
  EXPECT_CALL(*clientOutputSub, request_(1))
      .InSequence(s)
      // Client sends the initial request.
      .WillOnce(Invoke([&](size_t) {
        clientOutput->onNext(Payload(originalPayload->clone()));
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestChannel_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request, std::shared_ptr<Subscriber<Payload>> response) {
            serverOutput = response;
            serverOutput->onSubscribe(serverOutputSub);
            return serverInput;
          }));
  EXPECT_CALL(*serverInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
        serverInputSub = sub;
        // Client requests two payloads.
        clientInputSub->request(2);
      }));
  EXPECT_CALL(*serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      // Client receives the first payload and requests one.
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Client receives the second payload.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Server now requests two payloads.
  EXPECT_CALL(*serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t) { serverInputSub->request(2); }));
  EXPECT_CALL(*clientOutputSub, request_(2))
      .InSequence(s)
      // Client responds with the first one.
      .WillOnce(Invoke([&](size_t) {
        clientOutput->onNext(Payload(originalPayload->clone()));
      }));
  EXPECT_CALL(*serverInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      // Server sends one in return.
      .WillOnce(
          Invoke([&](Payload& p) { serverOutput->onNext(std::move(p)); }));
  // Client sends the second one.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(
          Invoke([&](Payload& p) { clientOutput->onNext(std::move(p)); }));
  Sequence s0, s1, s2, s3;
  EXPECT_CALL(*serverInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1, s2, s3)
      // Server closes the channel in response.
      .WillOnce(Invoke([&](Payload&) {
        serverOutput->onComplete();
        serverInputSub->cancel();
      }));
  EXPECT_CALL(*serverOutputSub, cancel_()).InSequence(s0);
  EXPECT_CALL(*serverInput, onComplete_()).InSequence(s1);
  EXPECT_CALL(*clientInput, onComplete_())
      .InSequence(s2)
      .WillOnce(Invoke([&]() { clientInputSub->cancel(); }));
  EXPECT_CALL(*clientOutputSub, cancel_())
      .InSequence(s3)
      .WillOnce(Invoke([&]() { clientOutput->onComplete(); }));

  // Kick off the magic.
  clientOutput = clientSock->requestChannel(clientInput);
  clientOutput->onSubscribe(clientOutputSub);
}

TEST(ReactiveSocketTest, RequestStreamComplete) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a stream
  EXPECT_CALL(*clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
        clientInputSub = sub;
        // Request two payloads immediately.
        clientInputSub->request(2);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request, std::shared_ptr<Subscriber<Payload>> response) {
            serverOutput = response;
            serverOutput->onSubscribe(serverOutputSub);
          }));
  EXPECT_CALL(*serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the first payload.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives the second payload and requests one more.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Server now sends one more payload with the complete bit set.
  EXPECT_CALL(*serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the third (and last) payload
  Sequence s0, s1;
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1)
      // Server closes the subscription by calling onComplete() in response
      // to sending the final item
      .WillOnce(Invoke([&](Payload&) {
        EXPECT_CALL(*serverOutputSub, cancel_()).InSequence(s0);
        serverOutput->onComplete();
      }));
  EXPECT_CALL(*clientInput, onComplete_())
      .InSequence(s1)
      .WillOnce(Invoke([&]() { clientInputSub->cancel(); }));

  // Kick off the magic.
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestStreamCancel) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a stream
  EXPECT_CALL(*clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
        clientInputSub = sub;
        // Request two payloads immediately.
        clientInputSub->request(2);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request, std::shared_ptr<Subscriber<Payload>> response) {
            serverOutput = response;
            serverOutput->onSubscribe(serverOutputSub);
          }));
  EXPECT_CALL(*serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the first payload.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives the second payload and requests one more.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Server now sends one more payload.
  EXPECT_CALL(*serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the third (and last) payload
  Sequence s0, s1;
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1)
      // Client closes the subscription in response.
      .WillOnce(Invoke([&](Payload&) { clientInputSub->cancel(); }));
  EXPECT_CALL(*serverOutputSub, cancel_())
      .InSequence(s0)
      .WillOnce(Invoke([&]() { serverOutput->onComplete(); }));
  EXPECT_CALL(*clientInput, onComplete_()).InSequence(s1);

  // Kick off the magic.
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestSubscription) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a subscription.
  EXPECT_CALL(*clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
        clientInputSub = sub;
        // Request two payloads immediately.
        clientInputSub->request(2);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestSubscription_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request, std::shared_ptr<Subscriber<Payload>> response) {
            serverOutput = response;
            serverOutput->onSubscribe(serverOutputSub);
          }));
  EXPECT_CALL(*serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the first payload.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives the second payload and requests one more.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Server now sends one more payload.
  EXPECT_CALL(*serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the third (and last) payload.
  Sequence s0, s1;
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1)
      // Client closes the subscription in response.
      .WillOnce(Invoke([&](Payload&) { clientInputSub->cancel(); }));
  EXPECT_CALL(*serverOutputSub, cancel_())
      .InSequence(s0)
      .WillOnce(Invoke([&]() { serverOutput->onComplete(); }));
  EXPECT_CALL(*clientInput, onComplete_()).InSequence(s1);

  // Kick off the magic.
  clientSock->requestSubscription(
      Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestSubscriptionSurplusResponse) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a subscription.
  EXPECT_CALL(*clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
        clientInputSub = sub;
        // Request one payload immediately.
        clientInputSub->request(1);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestSubscription_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              const std::shared_ptr<Subscriber<Payload>>& response) {
            serverOutput = response;
            serverOutput->onSubscribe(serverOutputSub);
          }));
  EXPECT_CALL(*serverOutputSub, request_(1))
      .InSequence(s)
      // The server delivers immediately, but an extra one.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));
  // Client receives the first payload.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives error instead of the second payload.
  EXPECT_CALL(*clientInput, onError_(_)).Times(1).InSequence(s);
  EXPECT_CALL(*clientInput, onComplete_()).Times(0);
  //  // Client closes the subscription in response.
  EXPECT_CALL(*serverOutputSub, cancel_()).InSequence(s).WillOnce(Invoke([&]() {
    serverOutput->onComplete();
  }));

  // Kick off the magic.
  clientSock->requestSubscription(
      Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestResponse) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a subscription.
  EXPECT_CALL(*clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
        clientInputSub = sub;
        // Request payload immediately.
        clientInputSub->request(1);
      }));

  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestResponse_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request, std::shared_ptr<Subscriber<Payload>> response) {
            serverOutput = response;
            serverOutput->onSubscribe(serverOutputSub);
          }));

  EXPECT_CALL(*serverOutputSub, request_(_))
      .InSequence(s)
      // The server deliver the response immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(Payload(originalPayload->clone()));
      }));

  // Client receives the only payload and closes the subscription in response.
  EXPECT_CALL(*clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->cancel(); }));
  // Client also receives onComplete() call since the response frame received
  // had COMPELTE flag set
  EXPECT_CALL(*clientInput, onComplete_()).InSequence(s);

  EXPECT_CALL(*serverOutputSub, cancel_()).InSequence(s).WillOnce(Invoke([&]() {
    serverOutput->onComplete();
  }));

  // Kick off the magic.
  clientSock->requestResponse(Payload(originalPayload->clone()), clientInput);
}
TEST(ReactiveSocketTest, RequestFireAndForget) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client sends a fire-and-forget
  EXPECT_CALL(
      serverHandlerRef, handleFireAndForgetRequest_(Equals(&originalPayload)))
      .InSequence(s);

  clientSock->requestFireAndForget(Payload(originalPayload->clone()));
}

TEST(ReactiveSocketTest, RequestMetadataPush) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client sends a fire-and-forget
  EXPECT_CALL(serverHandlerRef, handleMetadataPush_(Equals2(&originalPayload)))
      .InSequence(s);

  clientSock->metadataPush(originalPayload->clone());
}

TEST(ReactiveSocketTest, SetupData) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload(
          "text/plain", "tex/plain", Payload("meta", "data")));

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));
}

TEST(ReactiveSocketTest, Destructor) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  // TODO: since we don't assert anything, should we just use the StatsPrinter
  // instead?
  NiceMock<MockStats> clientStats;
  NiceMock<MockStats> serverStats;
  std::array<std::shared_ptr<StrictMock<MockSubscriber<Payload>>>, 2>
      clientInputs;
  clientInputs[0] = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  clientInputs[1] = std::make_shared<StrictMock<MockSubscriber<Payload>>>();

  std::array<std::shared_ptr<StrictMock<MockSubscription>>, 2> serverOutputSubs;
  serverOutputSubs[0] = std::make_shared<StrictMock<MockSubscription>>();
  serverOutputSubs[1] = std::make_shared<StrictMock<MockSubscription>>();

  std::array<std::shared_ptr<Subscription>, 2> clientInputSubs;
  std::array<std::shared_ptr<Subscriber<Payload>>, 2> serverOutputs;

  EXPECT_CALL(clientStats, socketCreated_()).Times(1);
  EXPECT_CALL(serverStats, socketCreated_()).Times(1);
  EXPECT_CALL(clientStats, socketClosed_()).Times(1);
  EXPECT_CALL(serverStats, socketClosed_()).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()),
      clientStats);

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_)).InSequence(s);

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler), serverStats);

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Two independent subscriptions.
  for (size_t i = 0; i < 2; ++i) {
    // Client creates a subscription.
    EXPECT_CALL(*clientInputs[i], onSubscribe_(_))
        .InSequence(s)
        .WillOnce(
            Invoke([i, &clientInputSubs](std::shared_ptr<Subscription> sub) {
              clientInputSubs[i] = sub;
              // Request two payloads immediately.
              sub->request(2);
            }));
    // The request reaches the other end and triggers new responder to be set
    // up.
    EXPECT_CALL(
        serverHandlerRef,
        handleRequestSubscription_(Equals(&originalPayload), _))
        .InSequence(s)
        .WillOnce(Invoke([i, &serverOutputs, &serverOutputSubs](
            Payload& request, std::shared_ptr<Subscriber<Payload>> response) {
          serverOutputs[i] = response;
          serverOutputs[i]->onSubscribe(serverOutputSubs[i]);
        }));
    Sequence s0, s1;
    EXPECT_CALL(*serverOutputSubs[i], request_(2))
        .InSequence(s, s0, s1)
        .WillOnce(Invoke([i, &serverSock](size_t) {
          if (i == 1) {
            // The second subscription tears down server-side instance of
            // ReactiveSocket immediately upon receiving request(n) signal.
            serverSock.reset();
          }
        }));
    // Subscriptions will be terminated by ReactiveSocket implementation.
    EXPECT_CALL(*serverOutputSubs[i], cancel_())
        .InSequence(s0)
        .WillOnce(
            Invoke([i, &serverOutputs]() { serverOutputs[i]->onComplete(); }));
    EXPECT_CALL(*clientInputs[i], onComplete_())
        .InSequence(s1)
        .WillOnce(
            Invoke([i, &clientInputSubs]() { clientInputSubs[i]->cancel(); }));
  }

  // Kick off the magic.
  for (size_t i = 0; i < 2; ++i) {
    clientSock->requestSubscription(
        Payload(originalPayload->clone()), clientInputs[i]);
  }

  //  clientSock.reset(nullptr);
  //  serverSock.reset(nullptr);
}

TEST(ReactiveSocketTest, ReactiveSocketOverInlineConnection) {
  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      ConnectionSetupPayload("", "", Payload()));

  // we don't expect any call other than setup payload
  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_));

  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));
}

class ReactiveSocketIgnoreRequestTest : public testing::Test {
 public:
  class TestRequestHandler : public NullRequestHandler {
    std::shared_ptr<Subscriber<Payload>> onRequestChannel(
        Payload /*request*/,
        SubscriberFactory& subscriberFactory) override {
      return std::make_shared<NullSubscriber>();
    }

    void onRequestStream(
        Payload /*request*/,
        SubscriberFactory& subscriberFactory) override {}

    void onRequestSubscription(
        Payload /*request*/,
        SubscriberFactory& subscriberFactory) override {}

    void onRequestResponse(
        Payload /*request*/,
        SubscriberFactory& subscriberFactory) override {}
  };

  ReactiveSocketIgnoreRequestTest() {
    auto clientConn = folly::make_unique<InlineConnection>();
    auto serverConn = folly::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    clientSock = ReactiveSocket::fromClientConnection(
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        folly::make_unique<StrictMock<MockRequestHandler>>(),
        ConnectionSetupPayload("", "", Payload()));

    serverSock = ReactiveSocket::fromServerConnection(
        std::move(serverConn), folly::make_unique<TestRequestHandler>());

    // Client request.
    EXPECT_CALL(*clientInput, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
          clientInputSub = sub;
          sub->request(2);
        }));

    //
    // server RequestHandler is ignoring the request, we expect terminating
    // response
    //

    EXPECT_CALL(*clientInput, onNext_(_)).Times(0);
    EXPECT_CALL(*clientInput, onComplete_()).WillOnce(Invoke([&]() {
      clientInputSub->cancel();
      clientInputSub = nullptr;
    }));
  }

  std::unique_ptr<ReactiveSocket> clientSock;
  std::unique_ptr<ReactiveSocket> serverSock;

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> clientInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> clientInputSub;

  const std::unique_ptr<folly::IOBuf> originalPayload{
      folly::IOBuf::copyBuffer("foo")};
};

TEST_F(ReactiveSocketIgnoreRequestTest, IgnoreRequestResponse) {
  clientSock->requestResponse(Payload(originalPayload->clone()), clientInput);
}

TEST_F(ReactiveSocketIgnoreRequestTest, IgnoreRequestStream) {
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
}

TEST_F(ReactiveSocketIgnoreRequestTest, IgnoreRequestSubscription) {
  clientSock->requestSubscription(
      Payload(originalPayload->clone()), clientInput);
}

TEST_F(ReactiveSocketIgnoreRequestTest, IgnoreRequestChannel) {
  auto clientOutput = clientSock->requestChannel(clientInput);

  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*clientOutputSub, request_(1)).WillOnce(Invoke([&](size_t) {
    clientOutput->onNext(Payload(originalPayload->clone()));
  }));
  EXPECT_CALL(*clientOutputSub, cancel_()).WillOnce(Invoke([&]() {
    clientOutput->onComplete();
    clientOutput = nullptr;
  }));

  clientOutput->onSubscribe(clientOutputSub);
}
