// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>
#include <gmock/gmock.h>
#include <array>
#include <chrono>
#include <thread>
#include "src/FrameTransport.h"
#include "src/NullRequestHandler.h"
#include "src/ReactiveSocket.h"
#include "src/folly/FollyKeepaliveTimer.h"
#include "test/InlineConnection.h"
#include "test/MockKeepaliveTimer.h"
#include "test/MockRequestHandler.h"
#include "test/MockStats.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;
using namespace std::string_literals;

MATCHER_P(
    Equals,
    payload,
    "Payloads " + std::string(negation ? "don't" : "") + "match") {
  return folly::IOBufEqual()(*payload, arg.data);
}

MATCHER_P(
    Equals2,
    payload,
    "Payloads " + std::string(negation ? "don't" : "") + "match") {
  return folly::IOBufEqual()(*payload, arg);
}

DECLARE_string(rs_use_protocol_version);

TEST(ReactiveSocketTest, RequestChannel) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub, serverInputSub;
  std::shared_ptr<Subscriber<Payload>> clientOutput, serverOutput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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
      serverHandlerRef, handleRequestChannel_(Equals(&originalPayload), _, _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              StreamId streamId,
              std::shared_ptr<Subscriber<Payload>> response) {
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

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _, _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              StreamId streamId,
              std::shared_ptr<Subscriber<Payload>> response) {
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

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _, _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              StreamId streamId,
              std::shared_ptr<Subscriber<Payload>> response) {
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

TEST(ReactiveSocketTest, RequestStream) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _, _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              StreamId streamId,
              std::shared_ptr<Subscriber<Payload>> response) {
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
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestStreamSendsOneRequest) {
  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();

  clientConn->connectTo(*serverConn);

  auto testInputSubscription = std::make_shared<MockSubscription>();

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));

  serverConn->setInput(testOutputSubscriber);
  auto sub = serverConn->getOutput();
  sub->onSubscribe(testInputSubscription);

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);
  EXPECT_CALL(*requestHandler, socketOnDisconnected(_)).Times(1);

  auto socket = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      std::move(requestHandler),
      ConnectionSetupPayload());

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  auto responseSubscriber = std::make_shared<MockSubscriber<Payload>>();
  std::shared_ptr<Subscription> clientInputSub;
  EXPECT_CALL(*responseSubscriber, onSubscribe_(_))
      .Times(1)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        clientInputSub = subscription;
      }));
  EXPECT_CALL(*testOutputSubscriber, onNext_(_)).Times(0);

  socket->requestStream(Payload(originalPayload->clone()), responseSubscriber);

  EXPECT_CALL(*testOutputSubscriber, onNext_(_))
      .Times(1)
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
        auto frameSerializer = FrameSerializer::createCurrentVersion();
        auto frameType = frameSerializer->peekFrameType(*frame);
        Frame_REQUEST_STREAM request;
        ASSERT_EQ(FrameType::REQUEST_STREAM, frameType);
        ASSERT_TRUE(
            frameSerializer->deserializeFrom(request, std::move(frame)));
        ASSERT_EQ("foo", request.payload_.moveDataToString());
        ASSERT_EQ((uint32_t)7, request.requestN_);
      }));

  clientInputSub->request(7);

  socket->disconnect();
  socket->close();
  sub->onComplete();
}

TEST(ReactiveSocketTest, RequestStreamSurplusResponse) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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
      serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _, _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              StreamId streamId,
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
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestResponse) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto clientInput = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  auto serverOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  std::shared_ptr<Subscription> clientInputSub;
  std::shared_ptr<Subscriber<Payload>> serverOutput;


  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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
      serverHandlerRef, handleRequestResponse_(Equals(&originalPayload), _, _))
      .InSequence(s)
      .WillOnce(Invoke(
          [&](Payload& request,
              StreamId streamId,
              std::shared_ptr<Subscriber<Payload>> response) {
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

  EXPECT_CALL(*serverOutputSub, cancel_()).WillOnce(Invoke([&]() {
    serverOutput->onComplete();
  }));

  // Kick off the magic.
  clientSock->requestResponse(Payload(originalPayload->clone()), clientInput);
}

TEST(ReactiveSocketTest, RequestResponseSendsOneRequest) {
  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();

  clientConn->connectTo(*serverConn);

  auto testInputSubscription = std::make_shared<MockSubscription>();

  auto testOutputSubscriber =
      std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
  EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        // allow receiving frames from the automaton
        subscription->request(std::numeric_limits<size_t>::max());
      }));

  serverConn->setInput(testOutputSubscriber);
  auto sub = serverConn->getOutput();
  sub->onSubscribe(testInputSubscription);

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);
  EXPECT_CALL(*requestHandler, socketOnDisconnected(_)).Times(1);

  auto socket = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      std::move(requestHandler),
      ConnectionSetupPayload());

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  auto responseSubscriber = std::make_shared<MockSubscriber<Payload>>();
  std::shared_ptr<Subscription> clientInputSub;
  EXPECT_CALL(*responseSubscriber, onSubscribe_(_))
      .Times(1)
      .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
        clientInputSub = subscription;
      }));
  EXPECT_CALL(*testOutputSubscriber, onNext_(_)).Times(0);

  socket->requestResponse(
      Payload(originalPayload->clone()), responseSubscriber);

  EXPECT_CALL(*testOutputSubscriber, onNext_(_))
      .Times(1)
      .WillOnce(Invoke([&](std::unique_ptr<folly::IOBuf>& frame) {
        auto frameSerializer = FrameSerializer::createCurrentVersion();
        auto frameType = frameSerializer->peekFrameType(*frame);
        Frame_REQUEST_RESPONSE request;
        ASSERT_EQ(FrameType::REQUEST_RESPONSE, frameType);
        ASSERT_TRUE(
            frameSerializer->deserializeFrom(request, std::move(frame)));
        ASSERT_EQ("foo", request.payload_.moveDataToString());
      }));

  clientInputSub->request(7);

  socket->disconnect();
  socket->close();
  sub->onComplete();
}

TEST(ReactiveSocketTest, RequestFireAndForget) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

  const auto originalPayload = folly::IOBuf::copyBuffer("foo");

  // Client sends a fire-and-forget
  EXPECT_CALL(
      serverHandlerRef,
      handleFireAndForgetRequest_(Equals(&originalPayload), _))
      .InSequence(s);

  clientSock->requestFireAndForget(Payload(originalPayload->clone()));
}

TEST(ReactiveSocketTest, RequestMetadataPush) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload(
          "text/plain", "text/plain", Payload("meta", "data")));

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));
}

TEST(ReactiveSocketTest, SetupWithKeepaliveAndStats) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  StrictMock<MockSubscriber<Payload>> clientInput;
  auto clientStats = std::make_shared<NiceMock<MockStats>>();
  ;
  std::unique_ptr<MockKeepaliveTimer> clientKeepalive =
      std::make_unique<MockKeepaliveTimer>();

  EXPECT_CALL(*clientKeepalive, keepaliveTime())
      .WillRepeatedly(Return(std::chrono::milliseconds(10)));
  EXPECT_CALL(*clientKeepalive, start(_)).InSequence(s);

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  EXPECT_CALL(*clientKeepalive, stop()).InSequence(s);

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload(
          "text/plain", "text/plain", Payload("meta", "data")),
      clientStats,
      std::move(clientKeepalive));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));
}

TEST(ReactiveSocketTest, GoodKeepalive) {
  // When a socket is closed with an error (e.g., no response to keepalive)
  // ensure that clients can reset their pointers and rsocket can finish
  // cleanups and exit
  folly::Baton<> baton;
  auto th = std::make_unique<folly::ScopedEventBaseThread>();

  auto serverConn = std::make_unique<InlineConnection>();
  std::unique_ptr<ReactiveSocket> clientSock;
  auto keepalive_time = 10;

  th->getEventBase()->runInEventBaseThreadAndWait([&]() {
    auto clientConn = std::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);
    auto clientStats = std::make_shared<NiceMock<MockStats>>();

    auto clientKeepalive = std::make_unique<FollyKeepaliveTimer>(
        *th->getEventBase(), std::chrono::milliseconds(keepalive_time));

    auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

    clientSock = ReactiveSocket::fromClientConnection(
        *th->getEventBase(),
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        std::move(requestHandler),
        ConnectionSetupPayload(
            "text/plain", "text/plain", Payload("meta", "data")),
        clientStats,
        std::move(clientKeepalive));

    clientSock->onClosed([&](const folly::exception_wrapper& ex) {
      // socket is closed, time to cleanup
      clientSock.reset();
      baton.post();
    });

  });
  // wait for keepalive failure to happen
  baton.wait();
  ASSERT_EQ(nullptr, clientSock);
}

TEST(ReactiveSocketTest, Destructor) {
  // InlineConnection forwards appropriate calls in-line, hence the order of
  // mock calls will be deterministic.
  Sequence s;

  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  // TODO: since we don't assert anything, should we just use the StatsPrinter
  // instead?
  auto clientStats = std::make_shared<NiceMock<MockStats>>();
  auto serverStats = std::make_shared<NiceMock<MockStats>>();
  std::array<std::shared_ptr<StrictMock<MockSubscriber<Payload>>>, 2>
      clientInputs;
  clientInputs[0] = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  clientInputs[1] = std::make_shared<StrictMock<MockSubscriber<Payload>>>();

  std::array<std::shared_ptr<StrictMock<MockSubscription>>, 2> serverOutputSubs;
  serverOutputSubs[0] = std::make_shared<StrictMock<MockSubscription>>();
  serverOutputSubs[1] = std::make_shared<StrictMock<MockSubscription>>();

  std::array<std::shared_ptr<Subscription>, 2> clientInputSubs;
  std::array<std::shared_ptr<Subscriber<Payload>>, 2> serverOutputs;

  EXPECT_CALL(*clientStats, socketCreated()).Times(1);
  EXPECT_CALL(*serverStats, socketCreated()).Times(1);
  EXPECT_CALL(*clientStats, socketClosed(_)).Times(1);
  EXPECT_CALL(*serverStats, socketClosed(_)).Times(1);

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()),
      clientStats);

  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .InSequence(s)
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(),
      std::move(serverConn),
      std::move(serverHandler),
      serverStats);

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
        serverHandlerRef, handleRequestStream_(Equals(&originalPayload), _, _))
        .InSequence(s)
        .WillOnce(Invoke([i, &serverOutputs, &serverOutputSubs](
            Payload& request,
            StreamId streamId,
            std::shared_ptr<Subscriber<Payload>> response) {
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
    EXPECT_CALL(*clientInputs[i], onError_(_))
        .InSequence(s1)
        .WillOnce(Invoke([i, &clientInputSubs](
            const folly::exception_wrapper& ex) { LOG(INFO) << ex.what(); }));
  }

  // Kick off the magic.
  for (size_t i = 0; i < 2; ++i) {
    clientSock->requestStream(
        Payload(originalPayload->clone()), clientInputs[i]);
  }

  //  clientSock.reset(nullptr);
  //  serverSock.reset(nullptr);
}

TEST(ReactiveSocketTest, ReactiveSocketOverInlineConnection) {
  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  // we don't expect any call other than setup payload
  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .WillRepeatedly(Return(nullptr));

  auto serverSock = ReactiveSocket::fromServerConnection(
      defaultExecutor(), std::move(serverConn), std::move(serverHandler));
}

TEST(ReactiveSocketTest, CloseWithError) {
  auto clientConn = std::make_unique<InlineConnection>();
  auto serverConn = std::make_unique<InlineConnection>();
  clientConn->connectTo(*serverConn);

  auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

  auto clientSock = ReactiveSocket::fromClientConnection(
      defaultExecutor(),
      std::move(clientConn),
      // No interactions on this mock, the client will not accept any requests.
      std::move(requestHandler),
      ConnectionSetupPayload("", "", Payload()));

  // We don't expect any call other than setup payload.
  auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
  EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
  auto& serverHandlerRef = *serverHandler;

  const folly::StringPiece errString{"Hahaha"};

  EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
      .WillRepeatedly(Invoke([&](auto& socket, auto&) {
        socket.closeConnectionError(errString.str());
        return nullptr;
      }));

  auto serverSock = ReactiveSocket::disconnectedServer(
      defaultExecutor(), std::move(serverHandler));

  serverSock->onClosed([&](auto const& exn) {
    EXPECT_TRUE(
        folly::StringPiece{exn.what().toStdString()}.contains(errString));
  });

  serverSock->serverConnect(
      std::make_shared<FrameTransport>(std::move(serverConn)),
      SocketParameters(false, FrameSerializer::getCurrentProtocolVersion()));
}

class ReactiveSocketIgnoreRequestTest : public testing::Test {
 public:
  ReactiveSocketIgnoreRequestTest() {
    auto clientConn = std::make_unique<InlineConnection>();
    auto serverConn = std::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

    clientSock = ReactiveSocket::fromClientConnection(
        defaultExecutor(),
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        std::move(requestHandler),
        ConnectionSetupPayload("", "", Payload()));

    serverSock = ReactiveSocket::fromServerConnection(
        defaultExecutor(),
        std::move(serverConn),
        std::make_unique<DefaultRequestHandler>());

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
    EXPECT_CALL(*clientInput, onComplete_()).Times(0);
    EXPECT_CALL(*clientInput, onError_(_))
        .WillOnce(Invoke([&](const folly::exception_wrapper& ex) {
          LOG(INFO) << "expected error: " << ex.what();
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

class ReactiveSocketOnErrorOnShutdownTest : public testing::Test {
 public:
  ReactiveSocketOnErrorOnShutdownTest() {
    auto clientConn = std::make_unique<InlineConnection>();
    auto serverConn = std::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
    EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

    clientSock = ReactiveSocket::fromClientConnection(
        defaultExecutor(),
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        std::move(requestHandler),
        ConnectionSetupPayload("", "", Payload()));

    auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
    EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
        .WillRepeatedly(Return(nullptr));

    serverSock = ReactiveSocket::fromServerConnection(
        defaultExecutor(), std::move(serverConn), std::move(serverHandler));

    EXPECT_CALL(*clientInput, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
          clientInputSub = sub;
          sub->request(2);
        }));

    EXPECT_CALL(serverHandlerRef, handleRequestResponse_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke([&](
            Payload& request,
            StreamId streamId,
            std::shared_ptr<Subscriber<Payload>> response) {
          serverOutput = response;
          serverOutput->onSubscribe(serverOutputSub);
          serverSock.reset(); // should close everything, but streams should end
          // with onError
        }));
    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke([&](
            Payload& request,
            StreamId streamId,
            std::shared_ptr<Subscriber<Payload>> response) {
          serverOutput = response;
          serverOutput->onSubscribe(serverOutputSub);
          serverSock.reset(); // should close everything, but streams should end
          // with onError
        }));
    EXPECT_CALL(serverHandlerRef, handleRequestChannel_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke([&](
            Payload& request,
            StreamId streamId,
            std::shared_ptr<Subscriber<Payload>> response) {

          EXPECT_CALL(*serverInput, onSubscribe_(_))
              .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
                serverInputSub = sub;
                sub->request(2);
              }));
          EXPECT_CALL(*serverInput, onComplete_()).Times(1);

          serverOutput = response;
          serverOutput->onSubscribe(serverOutputSub);
          serverSock.reset(); // should close everything, but streams should end
          // with onError

          return serverInput;
        }));

    EXPECT_CALL(*clientInput, onNext_(_)).Times(0);
    EXPECT_CALL(*clientInput, onComplete_()).Times(0);

    EXPECT_CALL(*serverOutputSub, cancel_()).WillOnce(Invoke([&]() {
      serverOutput->onComplete();
      serverOutput = nullptr;
    }));

    EXPECT_CALL(*clientInput, onError_(_))
        .WillOnce(Invoke([&](folly::exception_wrapper) {
          clientInputSub->cancel();
          clientInputSub = nullptr;
        }));
  }

  std::unique_ptr<ReactiveSocket> clientSock;
  std::unique_ptr<ReactiveSocket> serverSock;

  const std::unique_ptr<folly::IOBuf> originalPayload{
      folly::IOBuf::copyBuffer("foo")};

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> clientInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> clientInputSub;

  std::shared_ptr<Subscriber<Payload>> serverOutput;
  std::shared_ptr<StrictMock<MockSubscription>> serverOutputSub{
      std::make_shared<StrictMock<MockSubscription>>()};

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> serverInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> serverInputSub;
};

TEST_F(ReactiveSocketOnErrorOnShutdownTest, RequestResponse) {
  clientSock->requestResponse(Payload(originalPayload->clone()), clientInput);
}

TEST_F(ReactiveSocketOnErrorOnShutdownTest, RequestStream) {
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
}

TEST_F(ReactiveSocketOnErrorOnShutdownTest, RequestChannel) {
  auto clientOutput = clientSock->requestChannel(clientInput);

  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*clientOutputSub, request_(1)).WillOnce(Invoke([&](size_t) {
    // this will initiate the interaction
    clientOutput->onNext(Payload(originalPayload->clone()));
  }));
  EXPECT_CALL(*clientOutputSub, cancel_()).WillOnce(Invoke([&]() {
    clientOutput->onComplete();
    clientOutput = nullptr;
  }));

  clientOutput->onSubscribe(clientOutputSub);
}

using IOBufPtr = std::unique_ptr<folly::IOBuf>;

class MockDuplexConnection : public DuplexConnection {
 public:
  MOCK_METHOD1(setInput, void(std::shared_ptr<Subscriber<IOBufPtr>>));
  MOCK_METHOD0(getOutput, std::shared_ptr<Subscriber<IOBufPtr>>());
};

class ReactiveSocketRegressionTest : public Test {
 private:
  std::unique_ptr<StrictMock<MockRequestHandler>> requestHandlerPtr_;

 protected:
  ReactiveSocketRegressionTest()
      : requestHandlerPtr_(std::make_unique<StrictMock<MockRequestHandler>>()),
        requestHandler_(*requestHandlerPtr_) {
    auto connectionPtr = std::make_unique<MockDuplexConnection>();
    auto& connection = *connectionPtr;
    EXPECT_CALL(connection, setInput(_)).WillOnce(SaveArg<0>(&input_));
    EXPECT_CALL(connection, getOutput())
        .WillOnce(Return(std::make_shared<MockSubscriber<IOBufPtr>>()));

    EXPECT_CALL(requestHandler_, handleSetupPayload_(_, _))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(requestHandler_, socketOnConnected()).Times(1);
    EXPECT_CALL(requestHandler_, socketOnClosed(_)).Times(1);

    socket_ = ReactiveSocket::fromServerConnection(
        defaultExecutor(),
        std::move(connectionPtr),
        std::move(requestHandlerPtr_),
        Stats::noop(),
        SocketParameters(false, FrameSerializer::getCurrentProtocolVersion()));
  }

  MockRequestHandler& requestHandler_;
  std::shared_ptr<Subscriber<IOBufPtr>> input_;
  std::unique_ptr<ReactiveSocket> socket_;
};

TEST_F(ReactiveSocketRegressionTest, NoCrashOnUnknownStream) {
  input_->onNext(folly::IOBuf::copyBuffer(
      "\x00\x00\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00"s));
}

TEST_F(ReactiveSocketRegressionTest, MetadataFrameWithoutMetadataFlag) {
  // This is to make the expectation explicit. Technically it is not necessary
  // because requestHandler_ is a strict mock.
  EXPECT_CALL(requestHandler_, handleMetadataPush_(_)).Times(0);
  input_->onNext(folly::IOBuf::copyBuffer(
      "\x00\x0d\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"s));
}

class ReactiveSocketEmptyPayloadTest : public testing::Test {
 public:
  ReactiveSocketEmptyPayloadTest() {
    auto clientConn = std::make_unique<InlineConnection>();
    auto serverConn = std::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    auto connectionSetup = ConnectionSetupPayload("", "", Payload());
    connectionSetup.protocolVersion = ProtocolVersion(1, 0);

    auto requestHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    EXPECT_CALL(*requestHandler, socketOnConnected()).Times(1);
  EXPECT_CALL(*requestHandler, socketOnClosed(_)).Times(1);

    clientSock = ReactiveSocket::fromClientConnection(
        defaultExecutor(),
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        std::move(requestHandler),
        std::move(connectionSetup));

    auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    EXPECT_CALL(*serverHandler, socketOnConnected()).Times(1);
    EXPECT_CALL(*serverHandler, socketOnClosed(_)).Times(1);
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
        .WillRepeatedly(Return(nullptr));

    serverSock = ReactiveSocket::fromServerConnection(
        defaultExecutor(), std::move(serverConn), std::move(serverHandler));

    EXPECT_CALL(*clientInput, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
          clientInputSub = sub;
          sub->request(2);
        }));

    EXPECT_CALL(serverHandlerRef, handleRequestResponse_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              CHECK(!request) << "incoming request is expected to be empty";
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              CHECK(!request) << "incoming request is expected to be empty";
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestChannel_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              CHECK(!request) << "incoming request is expected to be empty";

              EXPECT_CALL(*serverInput, onSubscribe_(_))
                  .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
                    serverInputSub = sub;
                    sub->request(2);
                  }));

              EXPECT_CALL(*serverInput, onComplete_()).Times(1);
              EXPECT_CALL(*serverInput, onError_(_)).Times(0);

              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);

              return serverInput;
            }));

    EXPECT_CALL(*clientInput, onComplete_()).Times(1);
    EXPECT_CALL(*clientInput, onError_(_)).Times(0);

    EXPECT_CALL(*serverOutputSub, request_(_)).WillOnce(Invoke([&](size_t n) {
      CHECK_GT(n, 0);
      EXPECT_CALL(*clientInput, onNext_(_))
          .Times(n)
          .WillRepeatedly(Invoke([&](Payload& p) { CHECK(!p); }));
      while (n--) {
        serverOutput->onNext(Payload());
      }
      serverOutput->onComplete();
      serverOutput = nullptr;
    }));
  }

  std::unique_ptr<ReactiveSocket> clientSock;
  std::unique_ptr<ReactiveSocket> serverSock;

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> clientInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> clientInputSub;

  std::shared_ptr<Subscriber<Payload>> serverOutput;
  std::shared_ptr<NiceMock<MockSubscription>> serverOutputSub{
      std::make_shared<NiceMock<MockSubscription>>()};

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> serverInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> serverInputSub;
};

TEST_F(ReactiveSocketEmptyPayloadTest, RequestResponse) {
  clientSock->requestResponse(Payload(), clientInput);
}

TEST_F(ReactiveSocketEmptyPayloadTest, RequestStream) {
  clientSock->requestStream(Payload(), clientInput);
}

TEST_F(ReactiveSocketEmptyPayloadTest, RequestChannel) {
  auto clientOutput = clientSock->requestChannel(clientInput);
  auto clientOutputSub = std::make_shared<NiceMock<MockSubscription>>();

  int send = 3;
  EXPECT_CALL(*clientOutputSub, request_(_))
      .WillRepeatedly(Invoke([&](size_t n) {
        CHECK(n >= 1);
        while (n > 0 && send > 0) {
          clientOutput->onNext(Payload());
          --n;
          --send;
        }

        if (!send) {
          clientOutput->onComplete();
        }
      }));
  clientOutput->onSubscribe(clientOutputSub);
}