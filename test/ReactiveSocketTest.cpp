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
#include "src/Payload.h"
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
  return folly::IOBufEqual()(*payload, arg);
};

TEST(ReactiveSocketTest, RequestChannel) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  StrictMock<UnmanagedMockSubscriber<Payload>> clientInput, serverInput;
  StrictMock<UnmanagedMockSubscription> clientOutputSub, serverOutputSub;
  Subscription *clientInputSub = nullptr, *serverInputSub = nullptr;
  Subscriber<Payload> *clientOutput = nullptr, *serverOutput = nullptr;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a channel.
  EXPECT_CALL(clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](Subscription* sub) { clientInputSub = sub; }));
  // The initial payload is requested automatically.
  EXPECT_CALL(clientOutputSub, request_(1))
      .InSequence(s)
      // Client sends the initial request.
      .WillOnce(Invoke(
          [&](size_t) { clientOutput->onNext(originalPayload->clone()); }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestChannel_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload& request, Subscriber<Payload>* response) {
        serverOutput = response;
        serverOutput->onSubscribe(serverOutputSub);
        return &serverInput;
      }));
  EXPECT_CALL(serverInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](Subscription* sub) {
        serverInputSub = sub;
        // Client requests two payloads.
        clientInputSub->request(2);
      }));
  EXPECT_CALL(serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(originalPayload->clone());
        serverOutput->onNext(originalPayload->clone());
      }));
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      // Client receives the first payload and requests one.
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Client receives the second payload.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Server now requests two payloads.
  EXPECT_CALL(serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke([&](size_t) { serverInputSub->request(2); }));
  EXPECT_CALL(clientOutputSub, request_(2))
      .InSequence(s)
      // Client responds with the first one.
      .WillOnce(Invoke(
          [&](size_t) { clientOutput->onNext(originalPayload->clone()); }));
  EXPECT_CALL(serverInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      // Server sends one in return.
      .WillOnce(
          Invoke([&](Payload& p) { serverOutput->onNext(std::move(p)); }));
  // Client sends the second one.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(
          Invoke([&](Payload& p) { clientOutput->onNext(std::move(p)); }));
  Sequence s0, s1, s2, s3;
  EXPECT_CALL(serverInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1, s2, s3)
      // Server closes the channel in response.
      .WillOnce(Invoke([&](Payload&) {
        serverOutput->onComplete();
        serverInputSub->cancel();
      }));
  EXPECT_CALL(serverOutputSub, cancel_()).InSequence(s0);
  EXPECT_CALL(serverInput, onComplete_()).InSequence(s1);
  EXPECT_CALL(clientInput, onComplete_()).InSequence(s2).WillOnce(Invoke([&]() {
    clientInputSub->cancel();
  }));
  EXPECT_CALL(clientOutputSub, cancel_()).InSequence(s3).WillOnce(Invoke([&]() {
    clientOutput->onComplete();
  }));

  // Kick off the magic.
  clientOutput = &clientSock->requestChannel(clientInput);
  clientOutput->onSubscribe(clientOutputSub);
}

TEST(ReactiveSocketTest, RequestStreamComplete) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  StrictMock<UnmanagedMockSubscriber<Payload>> clientInput;
  StrictMock<UnmanagedMockSubscription> serverOutputSub;
  Subscription* clientInputSub = nullptr;
  Subscriber<Payload>* serverOutput = nullptr;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a stream
  EXPECT_CALL(clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](Subscription* sub) {
        clientInputSub = sub;
        // Request two payloads immediately.
        clientInputSub->request(2);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload& request, Subscriber<Payload>* response) {
        serverOutput = response;
        serverOutput->onSubscribe(serverOutputSub);
      }));
  EXPECT_CALL(serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(originalPayload->clone());
        serverOutput->onNext(originalPayload->clone());
      }));
  // Client receives the first payload.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives the second payload and requests one more.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Server now sends one more payload with the complete bit set.
  EXPECT_CALL(serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](size_t) { serverOutput->onNext(originalPayload->clone()); }));
  // Client receives the third (and last) payload
  Sequence s0, s1;
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1)
      // Server closes the subscription by calling onComplete() in response
      // to sending the final item
      .WillOnce(Invoke([&](Payload&) {
        EXPECT_CALL(serverOutputSub, cancel_()).InSequence(s0);
        serverOutput->onComplete();
      }));
  EXPECT_CALL(clientInput, onComplete_()).InSequence(s1).WillOnce(Invoke([&]() {
    clientInputSub->cancel();
  }));

  // Kick off the magic.
  clientSock->requestStream(originalPayload->clone(), clientInput);
}

TEST(ReactiveSocketTest, RequestStreamCancel) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  StrictMock<UnmanagedMockSubscriber<Payload>> clientInput;
  StrictMock<UnmanagedMockSubscription> serverOutputSub;
  Subscription* clientInputSub = nullptr;
  Subscriber<Payload>* serverOutput = nullptr;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a stream
  EXPECT_CALL(clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](Subscription* sub) {
        clientInputSub = sub;
        // Request two payloads immediately.
        clientInputSub->request(2);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload& request, Subscriber<Payload>* response) {
        serverOutput = response;
        serverOutput->onSubscribe(serverOutputSub);
      }));
  EXPECT_CALL(serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(originalPayload->clone());
        serverOutput->onNext(originalPayload->clone());
      }));
  // Client receives the first payload.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives the second payload and requests one more.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Server now sends one more payload.
  EXPECT_CALL(serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](size_t) { serverOutput->onNext(originalPayload->clone()); }));
  // Client receives the third (and last) payload
  Sequence s0, s1;
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1)
      // Client closes the subscription in response.
      .WillOnce(Invoke([&](Payload&) { clientInputSub->cancel(); }));
  EXPECT_CALL(serverOutputSub, cancel_()).InSequence(s0).WillOnce(Invoke([&]() {
    serverOutput->onComplete();
  }));
  EXPECT_CALL(clientInput, onComplete_()).InSequence(s1);

  // Kick off the magic.
  clientSock->requestStream(originalPayload->clone(), clientInput);
}

TEST(ReactiveSocketTest, RequestSubscription) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  StrictMock<UnmanagedMockSubscriber<Payload>> clientInput;
  StrictMock<UnmanagedMockSubscription> serverOutputSub;
  Subscription* clientInputSub = nullptr;
  Subscriber<Payload>* serverOutput = nullptr;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client creates a subscription.
  EXPECT_CALL(clientInput, onSubscribe_(_))
      .InSequence(s)
      .WillOnce(Invoke([&](Subscription* sub) {
        clientInputSub = sub;
        // Request two payloads immediately.
        clientInputSub->request(2);
      }));
  // The request reaches the other end and triggers new responder to be set up.
  EXPECT_CALL(
      serverHandlerRef, handleRequestSubscription_(Equals(&originalPayload), _))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload& request, Subscriber<Payload>* response) {
        serverOutput = response;
        serverOutput->onSubscribe(serverOutputSub);
      }));
  EXPECT_CALL(serverOutputSub, request_(2))
      .InSequence(s)
      // The server delivers them immediately.
      .WillOnce(Invoke([&](size_t) {
        serverOutput->onNext(originalPayload->clone());
        serverOutput->onNext(originalPayload->clone());
      }));
  // Client receives the first payload.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload))).InSequence(s);
  // Client receives the second payload and requests one more.
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s)
      .WillOnce(Invoke([&](Payload&) { clientInputSub->request(1); }));
  // Server now sends one more payload.
  EXPECT_CALL(serverOutputSub, request_(1))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](size_t) { serverOutput->onNext(originalPayload->clone()); }));
  // Client receives the third (and last) payload.
  Sequence s0, s1;
  EXPECT_CALL(clientInput, onNext_(Equals(&originalPayload)))
      .InSequence(s, s0, s1)
      // Client closes the subscription in response.
      .WillOnce(Invoke([&](Payload&) { clientInputSub->cancel(); }));
  EXPECT_CALL(serverOutputSub, cancel_()).InSequence(s0).WillOnce(Invoke([&]() {
    serverOutput->onComplete();
  }));
  EXPECT_CALL(clientInput, onComplete_()).InSequence(s1);

  // Kick off the magic.
  clientSock->requestSubscription(originalPayload->clone(), clientInput);
}

TEST(ReactiveSocketTest, RequestFireAndForget) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  StrictMock<UnmanagedMockSubscriber<Payload>> clientInput;
  StrictMock<UnmanagedMockSubscription> serverOutputSub;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client sends a fire-and-forget
  EXPECT_CALL(
      serverHandlerRef, handleFireAndForgetRequest_(Equals(&originalPayload)))
      .InSequence(s);

  clientSock->requestFireAndForget(originalPayload->clone());
}

TEST(ReactiveSocketTest, RequestMetadataPush) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  StrictMock<UnmanagedMockSubscriber<Payload>> clientInput;
  StrictMock<UnmanagedMockSubscription> serverOutputSub;

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>());

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client sends a fire-and-forget
  EXPECT_CALL(serverHandlerRef, handleMetadataPush_(Equals(&originalPayload)))
      .InSequence(s);

  clientSock->metadataPush(originalPayload->clone());
}

TEST(ReactiveSocketTest, Destructor) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = folly::make_unique<InlineConnection>();
  auto serverConn = folly::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn, true);

  // TODO: since we don't assert anything, should we just use the StatsPrinter
  // instead?
  NiceMock<MockStats> clientStats;
  NiceMock<MockStats> serverStats;
  std::array<StrictMock<UnmanagedMockSubscriber<Payload>>, 2> clientInputs;
  std::array<StrictMock<UnmanagedMockSubscription>, 2> serverOutputSubs;
  std::array<Subscription*, 2> clientInputSubs = {{nullptr}};
  std::array<Subscriber<Payload>*, 2> serverOutputs = {{nullptr}};

  EXPECT_CALL(clientStats, socketCreated_()).Times(1);
  EXPECT_CALL(serverStats, socketCreated_()).Times(1);
  EXPECT_CALL(clientStats, socketClosed_()).Times(1);
  EXPECT_CALL(serverStats, socketClosed_()).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      folly::make_unique<StrictMock<MockRequestHandler>>(),
      clientStats);

  auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
  auto& serverHandlerRef = *serverHandler;
  auto serverSock = ReactiveSocket::fromServerConnection(
      std::move(serverConn), std::move(serverHandler), serverStats);

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Two independent subscriptions.
  for (size_t i = 0; i < 2; ++i) {
    // Client creates a subscription.
    EXPECT_CALL(clientInputs[i], onSubscribe_(_))
        .InSequence(s)
        .WillOnce(Invoke([i, &clientInputSubs](Subscription* sub) {
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
            Payload& request, Subscriber<Payload>* response) {
          serverOutputs[i] = response;
          response->onSubscribe(serverOutputSubs[i]);
        }));
    Sequence s0, s1;
    EXPECT_CALL(serverOutputSubs[i], request_(2))
        .InSequence(s, s0, s1)
        .WillOnce(Invoke([i, &serverSock](size_t) {
          if (i == 1) {
            // The second subscription tears down server-side instance of
            // ReactiveSocket immediately upon receiving request(n) signal.
            serverSock.reset();
          }
        }));
    // Subscriptions will be terminated by ReactiveSocket implementation.
    EXPECT_CALL(serverOutputSubs[i], cancel_())
        .InSequence(s0)
        .WillOnce(
            Invoke([i, &serverOutputs]() { serverOutputs[i]->onComplete(); }));
    EXPECT_CALL(clientInputs[i], onComplete_())
        .InSequence(s1)
        .WillOnce(
            Invoke([i, &clientInputSubs]() { clientInputSubs[i]->cancel(); }));
  }

  // Kick off the magic.
  for (size_t i = 0; i < 2; ++i) {
    clientSock->requestSubscription(originalPayload->clone(), clientInputs[i]);
  }

  //  clientSock.reset(nullptr);
  //  serverSock.reset(nullptr);
}
