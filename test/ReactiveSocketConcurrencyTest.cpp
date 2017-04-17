// Copyright 2004-present Facebook. All Rights Reserved.

#include <array>
#include <chrono>
#include <condition_variable>
#include <thread>

#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
#include <folly/io/IOBuf.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <gmock/gmock.h>

#include "MockStats.h"
#include "src/StandardReactiveSocket.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/versions/FrameSerializer_v0_1.h"
#include "test/InlineConnection.h"
#include "test/MockRequestHandler.h"
#include "test/ReactiveStreamsMocksCompat.h"

using namespace ::testing;
using namespace ::reactivesocket;

class ClientSideConcurrencyTest : public testing::Test {
 public:
  ClientSideConcurrencyTest() {
    auto serverConn = std::make_unique<InlineConnection>();

    thread2.getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait([&] {
      auto clientConn = std::make_unique<InlineConnection>();
      clientConn->connectTo(*serverConn);
      clientSock = StandardReactiveSocket::fromClientConnection(
          *thread2.getEventBase(),
          std::move(clientConn),
          // No interactions on this mock, the client will not accept any
          // requests.
          std::make_unique<StrictMock<MockRequestHandler>>(),
          ConnectionSetupPayload("", "", Payload()),
          Stats::noop(),
          nullptr);
    });

    auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
        .WillRepeatedly(Return(nullptr));

    serverSock = StandardReactiveSocket::fromServerConnection(
        defaultExecutor(), std::move(serverConn), std::move(serverHandler));

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

  ~ClientSideConcurrencyTest() {
    auto socketMW = folly::makeMoveWrapper(clientSock);
    thread2.getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait(
        [socketMW]() mutable { socketMW->reset(); });
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

  static std::unique_ptr<folly::IOBuf> originalPayload() {
    return folly::IOBuf::copyBuffer("foo");
  }

  // we want these to be the first members, to be destroyed as last
  folly::ScopedEventBaseThread thread1;
  folly::ScopedEventBaseThread thread2;

  std::unique_ptr<StandardReactiveSocket> clientSock;
  std::unique_ptr<StandardReactiveSocket> serverSock;

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
};

TEST_F(ClientSideConcurrencyTest, RequestResponseTest) {
  thread2.getEventBase()->runInEventBaseThread([&] {
    clientSock->requestResponse(Payload(originalPayload()), clientInput);
  });
  wainUntilDone();
  LOG(INFO) << "test done";
}

TEST_F(ClientSideConcurrencyTest, RequestStreamTest) {
  thread2.getEventBase()->runInEventBaseThread([&] {
    clientSock->requestStream(Payload(originalPayload()), clientInput);
  });
  wainUntilDone();
}

TEST_F(ClientSideConcurrencyTest, RequestChannelTest) {
  clientTerminatesInteraction_ = false;

  std::shared_ptr<Subscriber<Payload>> clientOutput;
  thread2.getEventBase()->runInEventBaseThreadAndWait([&clientOutput, this] {
    clientOutput = clientSock->requestChannel(clientInput);
  });

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

  thread1.getEventBase()->runInEventBaseThread(
      [clientOutput, clientOutputSub]() {
        clientOutput->onSubscribe(clientOutputSub);
      });

  wainUntilDone();
}

class ServerSideConcurrencyTest : public testing::Test {
 public:
  ServerSideConcurrencyTest() {
    auto clientConn = std::make_unique<InlineConnection>();
    auto serverConn = std::make_unique<InlineConnection>();
    clientConn->connectTo(*serverConn);

    clientSock = StandardReactiveSocket::fromClientConnection(
        defaultExecutor(),
        std::move(clientConn),
        // No interactions on this mock, the client will not accept any
        // requests.
        std::make_unique<StrictMock<MockRequestHandler>>(),
        ConnectionSetupPayload("", "", Payload()));

    auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
        .WillRepeatedly(Return(nullptr));

    thread2.getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait([&] {
      serverSock = StandardReactiveSocket::fromServerConnection(
          *thread2.getEventBase(),
          std::move(serverConn),
          std::move(serverHandler),
          Stats::noop(),
          SocketParameters(false, ProtocolVersion::Unknown));
    });

    EXPECT_CALL(*clientInput, onSubscribe_(_))
        .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
          clientInputSub = sub;
          sub->request(3);
        }));
    // The request reaches the other end and triggers new responder to be set
    // up.
    EXPECT_CALL(serverHandlerRef, handleRequestResponse_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                const std::shared_ptr<Subscriber<Payload>>& response) {
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                const std::shared_ptr<Subscriber<Payload>>& response) {
              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);
            }));
    EXPECT_CALL(serverHandlerRef, handleRequestChannel_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                const std::shared_ptr<Subscriber<Payload>>& response) {
              clientTerminatesInteraction_ = false;

              EXPECT_CALL(*serverInput, onSubscribe_(_))
                  .WillOnce(Invoke([&](std::shared_ptr<Subscription> sub) {
                    serverInputSub = sub;
                    thread1.getEventBase()->runInEventBaseThreadAndWait(
                        [&]() { sub->request(2); });
                  }));

              // TODO(t15917213): Re-enable this assertion!
              // EXPECT_CALL(*serverInput, onNext_(_)).Times(1);

              // because we cancel the stream in onSubscribe
              EXPECT_CALL(*serverInput, onComplete_()).WillOnce(Invoke([&]() {
                EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
              }));

              serverOutput = response;
              serverOutput->onSubscribe(serverOutputSub);

              return serverInput;
            }));

    EXPECT_CALL(*serverOutputSub, request_(_))
        // The server delivers them immediately.
        .WillOnce(Invoke([&](size_t) {
          thread1.getEventBase()->runInEventBaseThreadAndWait(
              [&]() { serverOutput->onNext(Payload(originalPayload())); });
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
          EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
          // cancel is called from the thread1
          // but delivered on thread2
          if (clientTerminatesInteraction_) {
            clientInputSub->cancel();
            clientInputSub = nullptr;
            done();
          }
        }));

    EXPECT_CALL(*serverOutputSub, cancel_()).WillRepeatedly(Invoke([&]() {
      EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
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

  ~ServerSideConcurrencyTest() {
    auto socketMW = folly::makeMoveWrapper(serverSock);
    thread2.getEventBase()->runImmediatelyOrRunInEventBaseThreadAndWait(
        [socketMW]() mutable { socketMW->reset(); });
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

  // we want these to be the first members to be destroyed as last
  folly::ScopedEventBaseThread thread1;
  folly::ScopedEventBaseThread thread2;

  static std::unique_ptr<folly::IOBuf> originalPayload() {
    return folly::IOBuf::copyBuffer("foo");
  }

  std::unique_ptr<StandardReactiveSocket> clientSock;
  std::unique_ptr<StandardReactiveSocket> serverSock;

  std::shared_ptr<StrictMock<MockSubscriber<Payload>>> clientInput{
      std::make_shared<StrictMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> clientInputSub;

  std::shared_ptr<Subscriber<Payload>> serverOutput;
  std::shared_ptr<StrictMock<MockSubscription>> serverOutputSub{
      std::make_shared<StrictMock<MockSubscription>>()};

  std::shared_ptr<NiceMock<MockSubscriber<Payload>>> serverInput{
      std::make_shared<NiceMock<MockSubscriber<Payload>>>()};
  std::shared_ptr<Subscription> serverInputSub;

  bool clientTerminatesInteraction_{true};

  std::mutex mtx;
  std::condition_variable cv;
  bool isDone_{false};
};

TEST_F(ServerSideConcurrencyTest, RequestResponseTest) {
  clientSock->requestResponse(Payload(originalPayload()), clientInput);
  wainUntilDone();
}

TEST_F(ServerSideConcurrencyTest, RequestStreamTest) {
  clientSock->requestStream(Payload(originalPayload()), clientInput);
  wainUntilDone();
}

TEST_F(ServerSideConcurrencyTest, RequestChannelTest) {
  auto clientOutput = clientSock->requestChannel(clientInput);

  auto clientOutputSub = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*clientOutputSub, request_(1))
      .WillOnce(Invoke([clientOutput](size_t n) {
        // first payload for the server RequestHandler
        clientOutput->onNext(Payload(originalPayload()));
      }));
  EXPECT_CALL(*clientOutputSub, request_(2))
      .WillOnce(Invoke([clientOutput, this](size_t n) {
        EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
        // second payload for the server input subscriber
        clientOutput->onNext(Payload(originalPayload()));
      }));
  EXPECT_CALL(*clientOutputSub, cancel_())
      .WillOnce(Invoke([clientOutput, this]() {
        EXPECT_TRUE(thread2.getEventBase()->isInEventBaseThread());
        clientOutput->onComplete();
      }));

  clientOutput->onSubscribe(clientOutputSub);
  clientOutput = nullptr;

  wainUntilDone();
}

class InitialRequestNDeliveredTest : public testing::Test {
 public:
  InitialRequestNDeliveredTest() {
    auto serverSocketConnection = std::make_unique<InlineConnection>();
    auto testInlineConnection = std::make_unique<InlineConnection>();

    serverSocketConnection->connectTo(*testInlineConnection);

    testConnection = std::make_unique<FramedDuplexConnection>(
        std::move(testInlineConnection), inlineExecutor());

    testConnectionSub = testConnection->getOutput();
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
    testConnectionSub->onSubscribe(testInputSubscription);

    validatingSubscription = std::make_shared<MockSubscription>();

    EXPECT_CALL(*validatingSubscription, request_(_))
        .WillOnce(Invoke([&](size_t n) {
          EXPECT_EQ(expectedRequestN, n);
          serverSocket.reset();
        }));

    auto serverHandler = std::make_unique<StrictMock<MockRequestHandler>>();
    auto& serverHandlerRef = *serverHandler;

    EXPECT_CALL(serverHandlerRef, handleSetupPayload_(_, _))
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(serverHandlerRef, handleRequestStream_(_, _, _))
        .Times(AtMost(1))
        .WillOnce(Invoke(
            [&](Payload& request,
                StreamId streamId,
                const std::shared_ptr<Subscriber<Payload>>& response) {
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
                const std::shared_ptr<Subscriber<Payload>>& response) {
              thread2.getEventBase()->runInEventBaseThread([response, this] {
                /* sleep override */ std::this_thread::sleep_for(
                    std::chrono::milliseconds(5));
                response->onSubscribe(validatingSubscription);
              });
            }));

    serverSocket = StandardReactiveSocket::fromServerConnection(
        eventBase_,
        std::make_unique<FramedDuplexConnection>(
            std::move(serverSocketConnection), inlineExecutor()),
        std::move(serverHandler),
        Stats::noop(),
        SocketParameters(false, ProtocolVersion::Unknown));

    Frame_SETUP frameSetup(
        FrameFlags::EMPTY,
        0,
        1,
        Frame_SETUP::kMaxKeepaliveTime,
        Frame_SETUP::kMaxLifetime,
        ResumeIdentificationToken::generateNew(),
        "",
        "",
        Payload());
    testConnectionSub->onNext(
        FrameSerializer::createCurrentVersion()->serializeOut(
            std::move(frameSetup)));
  }

  void loopEventBaseUntilDone() {
    while (!done) {
      eventBase_.loop();
    }
  }

  // we want these to be the first members, to be destroyed as the last
  folly::ScopedEventBaseThread thread2;

  std::unique_ptr<StandardReactiveSocket> serverSocket;
  std::shared_ptr<MockSubscription> testInputSubscription;
  std::unique_ptr<DuplexConnection> testConnection;
  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> testConnectionSub;
  std::shared_ptr<MockSubscription> validatingSubscription;

  const size_t kStreamId{1};
  const size_t kRequestN{500};

  std::atomic<bool> done{false};
  size_t expectedRequestN{kRequestN};
  folly::EventBase eventBase_;
  FrameSerializerV0_1 frameSerializer;
};

TEST_F(InitialRequestNDeliveredTest, RequestResponse) {
  expectedRequestN = 1;
  Frame_REQUEST_RESPONSE requestFrame(kStreamId, FrameFlags::EMPTY, Payload());
  testConnectionSub->onNext(
      frameSerializer.serializeOut(std::move(requestFrame)));
  loopEventBaseUntilDone();
}

TEST_F(InitialRequestNDeliveredTest, RequestStream) {
  Frame_REQUEST_STREAM requestFrame(
      kStreamId, FrameFlags::EMPTY, kRequestN, Payload());
  testConnectionSub->onNext(
      frameSerializer.serializeOut(std::move(requestFrame)));
  loopEventBaseUntilDone();
}
