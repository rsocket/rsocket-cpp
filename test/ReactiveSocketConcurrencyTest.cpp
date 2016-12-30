// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>
#include <chrono>
#include <condition_variable>
#include <thread>

#include <folly/Memory.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>

#include "MockStats.h"
#include "src/ReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "test/InlineConnection.h"
#include "test/MockRequestHandler.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

class ClientSideConcurrencyTest : public testing::Test {
 public:
  ClientSideConcurrencyTest() {
    auto clientConn = folly::make_unique<InlineConnection>();
    auto serverConn = folly::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    clientSock = ReactiveSocket::fromClientConnection(
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        folly::make_unique<StrictMock<MockRequestHandler>>(),
        ConnectionSetupPayload("", "", Payload()));

    auto serverHandler = folly::make_unique<StrictMock<MockRequestHandler>>();
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_))
        .WillRepeatedly(Return(std::make_shared<StreamState>()));

    serverSock = ReactiveSocket::fromServerConnection(
        std::move(serverConn), std::move(serverHandler));

    EXPECT_CALL(*clientInput, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
          clientInputSub = sub;
          // request is called from the thread1
          // but delivered on thread2
          thread1.getEventBase()->runInEventBaseThreadAndWait(
              [&]() { sub->request(2); });
        }));
    // The request reaches the other end and triggers new responder to be set
    // up.
    EXPECT_CALL(serverHandlerRef, handleRequestResponse_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestSubscription_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestChannel_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                std::shared_ptr<Subscriber<Payload>> response) {
              EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());

              EXPECT_CALL(*serverInput, onSubscribe_(_))
                  .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
                    EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
                    serverInputSub = sub;
                    sub->request(2);
                  }));
              EXPECT_CALL(*serverInput, onNext_(_))
                  .WillOnce(Invoke([&](Payload& payload) {
                    EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
                    serverInputSub->cancel();
                    serverInputSub = nullptr;
                  }));
              EXPECT_CALL(*serverInput, onComplete_()).WillOnce(Invoke([&]() {
                EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
              }));

              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);

              return serverInput;
            }));

    EXPECT_CALL(*serverOutputSub, request_(_))
        // The server delivers them immediately.
        .WillOnce(Invoke(
            [&](size_t) { serverOutput->onNext(Payload(originalPayload())); }));
    EXPECT_CALL(*clientInput, onNext_(_))
        // Client receives the payload. We will complete the interaction
        .WillOnce(Invoke([&](Payload&) {
          // cancel is called from the thread1
          // but delivered on thread2
          if (clientTerminatesInteraction_) {
            thread1.getEventBase()->runInEventBaseThreadAndWait([&]() {
              clientInputSub->cancel();
              clientInputSub = nullptr;
              done();
            });
          }
        }));

    EXPECT_CALL(*serverOutputSub, cancel_()).WillOnce(Invoke([&]() {
      serverOutput->onComplete();
      serverOutput = nullptr;
    }));

    EXPECT_CALL(*clientInput, onComplete_()).WillOnce(Invoke([&]() {
      if (!clientTerminatesInteraction_) {
        clientInputSub->cancel();
        clientInputSub = nullptr;
        done();
      }
    }));
  }

  void done() {
    std::unique_lock<std::mutex> lock(mtx);
    isDone_ = true;
    cv.notify_one();
  }

  void wainUntilDone() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]() { return isDone_; });
  }

  std::unique_ptr<ReactiveSocket> clientSock;
  std::unique_ptr<ReactiveSocket> serverSock;

  static std::unique_ptr<folly::IOBuf> originalPayload() {
    return folly::IOBuf::copyBuffer("foo");
  };

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> clientInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> clientInputSub;

  std::shared_ptr<Subscriber<Payload>> serverOutput;
  std::shared_ptr<StrictMock<MockSubscription>> serverOutputSub{
      std::make_shared<StrictMock<MockSubscription>>()};

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> serverInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> serverInputSub;

  bool clientTerminatesInteraction_{true};

  std::mutex mtx;
  std::condition_variable cv;
  bool isDone_{false};

  // we want these to be the last members
  folly::ScopedEventBaseThread thread1;
  folly::ScopedEventBaseThread thread2;
};

TEST_F(ClientSideConcurrencyTest, RequestResponseTest) {
  clientSock->requestResponse(
      Payload(originalPayload()), clientInput, *thread2.getEventBase());
  wainUntilDone();
}

TEST_F(ClientSideConcurrencyTest, RequestStreamTest) {
  clientSock->requestStream(
      Payload(originalPayload()), clientInput, *thread2.getEventBase());
  wainUntilDone();
}

TEST_F(ClientSideConcurrencyTest, RequestSubscriptionTest) {
  clientSock->requestSubscription(
      Payload(originalPayload()), clientInput, *thread2.getEventBase());
  wainUntilDone();
}

TEST_F(ClientSideConcurrencyTest, RequestChannelTest) {
  clientTerminatesInteraction_ = false;

  auto clientOutput =
      clientSock->requestChannel(clientInput, *thread2.getEventBase());

  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*clientOutputSub, request_(1)).WillOnce(Invoke([&](size_t) {
    thread1.getEventBase()->runInEventBaseThread([clientOutput]() {
      // first payload for the server RequestHandler
      clientOutput->onNext(Payload(originalPayload()));
    });
  }));
  EXPECT_CALL(*clientOutputSub, request_(2))
      .WillOnce(Invoke([clientOutput](size_t) {
        // second payload for the server input subscriber
        clientOutput->onNext(Payload(originalPayload()));
      }));
  EXPECT_CALL(*clientOutputSub, cancel_()).Times(1);

  thread1.getEventBase()->runInEventBaseThreadAndWait(
      [&]() { clientOutput->onSubscribe(clientOutputSub); });

  wainUntilDone();
}

class ServerSideConcurrencyTest : public testing::Test {
 public:
  ServerSideConcurrencyTest() {
    auto clientConn = folly::make_unique<InlineConnection>();
    auto serverConn = folly::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    clientSock = ReactiveSocket::fromClientConnection(
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        folly::make_unique<StrictMock<MockRequestHandler>>(),
        ConnectionSetupPayload("", "", Payload()));

    auto serverHandler =
        folly::make_unique<StrictMock<MockRequestHandlerBase>>();
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_))
        .WillRepeatedly(Return(std::make_shared<StreamState>()));

    serverSock = ReactiveSocket::fromServerConnection(
        std::move(serverConn), std::move(serverHandler));

    EXPECT_CALL(*clientInput, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
          clientInputSub = sub;
          sub->request(2);
        }));
    // The request reaches the other end and triggers new responder to be set
    // up.
    EXPECT_CALL(serverHandlerRef, handleRequestResponse_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              serverOutput =
                  subscriberFactory.createSubscriber(*thread2.getEventBase());
              // TODO: should onSubscribe be queued and also called from
              // thread1?
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              serverOutput =
                  subscriberFactory.createSubscriber(*thread2.getEventBase());
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestSubscription_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              serverOutput =
                  subscriberFactory.createSubscriber(*thread2.getEventBase());
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestChannel_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              clientTerminatesInteraction_ = false;

              EXPECT_CALL(*serverInput, onSubscribe_(_))
                  .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
                    serverInputSub = sub;
                    thread1.getEventBase()->runInEventBaseThreadAndWait(
                        [&]() { sub->request(2); });
                  }));
              EXPECT_CALL(*serverInput, onNext_(_))
                  .WillOnce(Invoke([&](Payload& payload) {}));
              EXPECT_CALL(*serverInput, onComplete_()).WillOnce(Invoke([&]() {
                EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
              }));

              serverOutput =
                  subscriberFactory.createSubscriber(*thread2.getEventBase());
              serverOutput->onSubscribe(serverOutputSub);

              return serverInput;
            }));

    EXPECT_CALL(*serverOutputSub, request_(_))
        // The server delivers them immediately.
        .WillOnce(Invoke([&](size_t) {
          thread1.getEventBase()->runInEventBaseThreadAndWait([&]() {
            serverOutput->onNext(Payload(originalPayload->clone()));
          });
          thread1.getEventBase()->runInEventBaseThreadAndWait([&]() {
            if (serverInputSub) {
              serverInputSub->cancel();
              serverInputSub = nullptr;
            }
          });
        }));
    EXPECT_CALL(*clientInput, onNext_(_))
        // Client receives the payload. We will complete the interaction
        .WillOnce(Invoke([&](Payload&) {
          if (clientTerminatesInteraction_) {
            clientInputSub->cancel();
            clientInputSub = nullptr;
            done();
          }
        }));

    EXPECT_CALL(*serverOutputSub, cancel_()).WillOnce(Invoke([&]() {
      serverOutput->onComplete();
      serverOutput = nullptr;
    }));

    EXPECT_CALL(*clientInput, onComplete_()).WillOnce(Invoke([&]() {
      if (!clientTerminatesInteraction_) {
        clientInputSub->cancel();
        clientInputSub = nullptr;
        done();
      }
    }));
  }

  void done() {
    std::unique_lock<std::mutex> lock(mtx);
    isDone_ = true;
    cv.notify_one();
  }

  void wainUntilDone() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [&]() { return isDone_; });
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

  bool clientTerminatesInteraction_{true};

  std::mutex mtx;
  std::condition_variable cv;
  bool isDone_{false};

  // we want these to be the last members
  folly::ScopedEventBaseThread thread1;
  folly::ScopedEventBaseThread thread2;
};

TEST_F(ServerSideConcurrencyTest, RequestResponseTest) {
  clientSock->requestResponse(Payload(originalPayload->clone()), clientInput);
  wainUntilDone();
}

TEST_F(ServerSideConcurrencyTest, RequestStreamTest) {
  clientSock->requestStream(Payload(originalPayload->clone()), clientInput);
  wainUntilDone();
}

TEST_F(ServerSideConcurrencyTest, RequestSubscriptionTest) {
  clientSock->requestSubscription(
      Payload(originalPayload->clone()), clientInput);
  wainUntilDone();
}

TEST_F(ServerSideConcurrencyTest, RequestChannelTest) {
  auto clientOutput = clientSock->requestChannel(clientInput);

  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*clientOutputSub, request_(1)).WillOnce(Invoke([&](size_t n) {
    // first payload for the server RequestHandler
    clientOutput->onNext(Payload(originalPayload->clone()));
  }));
  EXPECT_CALL(*clientOutputSub, request_(2)).WillOnce(Invoke([&](size_t n) {
    EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
    // second payload for the server input subscriber
    clientOutput->onNext(Payload(originalPayload->clone()));
  }));
  EXPECT_CALL(*clientOutputSub, cancel_()).WillOnce(Invoke([&]() {
    EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
    clientOutput->onComplete();
    clientOutput = nullptr;
  }));

  clientOutput->onSubscribe(clientOutputSub);

  wainUntilDone();
}

class InitialRequestNDeliveredTest : public testing::Test {
 public:
  InitialRequestNDeliveredTest() {
    auto serverSocketConnection = folly::make_unique<InlineConnection>();
    auto testInlineConnection = folly::make_unique<InlineConnection>();

    serverSocketConnection->connectTo(*testInlineConnection);

    testConnection = folly::make_unique<FramedDuplexConnection>(
        std::move(testInlineConnection));

    testInputSubscription = std::make_shared<MockSubscription>();

    auto testOutputSubscriber =
        std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
    EXPECT_CALL(*testOutputSubscriber, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
          // allow receiving frames from the automaton
          subscription->request(std::numeric_limits<size_t>::max());
        }));
    EXPECT_CALL(*testOutputSubscriber, onComplete_()).WillOnce(Invoke([&]() {
      done = true;
    }));

    testConnection->setInput(testOutputSubscriber);
    testConnection->getOutput()->onSubscribe(testInputSubscription);

    validatingSubscription = std::make_shared<MockSubscription>();

    EXPECT_CALL(*validatingSubscription, request_(_))
        .WillOnce(Invoke([&](size_t n) {
          EXPECT_EQ(expectedRequestN, n);
          serverSocket.reset();
        }));

    auto serverHandler =
        folly::make_unique<StrictMock<MockRequestHandlerBase>>();
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_))
        .WillRepeatedly(Return(std::make_shared<StreamState>()));

    EXPECT_CALL(serverHandlerRef, handleRequestSubscription_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              auto response = subscriberFactory.createSubscriber(eventBase_);
              thread2.getEventBase()->runInEventBaseThread([response, this] {
                /* sleep override */ std::this_thread::sleep_for(
                    std::chrono::milliseconds(5));
                response->onSubscribe(validatingSubscription);
              });
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              auto response = subscriberFactory.createSubscriber(eventBase_);
              thread2.getEventBase()->runInEventBaseThread([response, this] {
                /* sleep override */ std::this_thread::sleep_for(
                    std::chrono::milliseconds(5));
                response->onSubscribe(validatingSubscription);
              });
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestResponse_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                SubscriberFactory& subscriberFactory) {
              auto response = subscriberFactory.createSubscriber(eventBase_);
              thread2.getEventBase()->runInEventBaseThread([response, this] {
                /* sleep override */ std::this_thread::sleep_for(
                    std::chrono::milliseconds(5));
                response->onSubscribe(validatingSubscription);
              });
            }));

    serverSocket = ReactiveSocket::fromServerConnection(
        folly::make_unique<FramedDuplexConnection>(
            std::move(serverSocketConnection)),
        std::move(serverHandler));

    testConnection->getOutput()->onNext(
        Frame_SETUP(
            FrameFlags_EMPTY,
            0,
            0,
            0,
            ResumeIdentificationToken::generateNew(),
            "",
            "",
            Payload())
            .serializeOut());
  }

  void loopEventBaseUntilDone() {
    while (!done) {
      eventBase_.loop();
    }
  }

  std::unique_ptr<ReactiveSocket> serverSocket;
  std::shared_ptr<MockSubscription> testInputSubscription;
  std::unique_ptr<DuplexConnection> testConnection;
  std::shared_ptr<MockSubscription> validatingSubscription;

  const size_t kStreamId{1};
  const size_t kRequestN{500};

  std::atomic<bool> done{false};
  size_t expectedRequestN{kRequestN};
  folly::EventBase eventBase_;

  // we want these to be the last members
  folly::ScopedEventBaseThread thread2;
};

TEST_F(InitialRequestNDeliveredTest, RequestResponse) {
  expectedRequestN = 1;
  Frame_REQUEST_RESPONSE requestFrame(kStreamId, FrameFlags_EMPTY, Payload());
  testConnection->getOutput()->onNext(requestFrame.serializeOut());
  loopEventBaseUntilDone();
}

TEST_F(InitialRequestNDeliveredTest, RequestStream) {
  Frame_REQUEST_STREAM requestFrame(
      kStreamId, FrameFlags_EMPTY, kRequestN, Payload());
  testConnection->getOutput()->onNext(requestFrame.serializeOut());
  loopEventBaseUntilDone();
}

TEST_F(InitialRequestNDeliveredTest, RequestSubscription) {
  Frame_REQUEST_SUB requestFrame(
      kStreamId, FrameFlags_EMPTY, kRequestN, Payload());
  testConnection->getOutput()->onNext(requestFrame.serializeOut());
  loopEventBaseUntilDone();
}
