// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ServerConnectionAcceptor.h"
#include "src/ConnectionSetupPayload.h"
#include "src/FrameTransport.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/FrameSerializer.h"

#include "test/ReactiveStreamsMocksCompat.h"
#include "test/InlineConnection.h"

#include <folly/ExceptionWrapper.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace reactivesocket;
using namespace testing;

class MockConnectionHandler : public ConnectionHandler {
public:
  void setupNewSocket(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload) override {
    doSetupNewSocket(std::move(frameTransport), setupPayload);
  }

  MOCK_METHOD2(
    doSetupNewSocket,
    void(std::shared_ptr<FrameTransport>, ConnectionSetupPayload&));

  MOCK_METHOD2(
    resumeSocket,
    bool(
      std::shared_ptr<FrameTransport>,
      ResumeParameters));

  MOCK_METHOD2(
    connectionError,
    void(std::shared_ptr<FrameTransport>, folly::exception_wrapper ex));
};

class ServerConnectionAcceptorTest : public Test {
public:
 ServerConnectionAcceptorTest() : acceptor_(ProtocolVersion::Unknown) {
   handler_ = std::make_shared<StrictMock<MockConnectionHandler>>();

   clientConnection_ = std::make_unique<InlineConnection>();
   serverConnection_ = std::make_unique<InlineConnection>();
   clientConnection_->connectTo(*serverConnection_);

   auto testInputSubscription = std::make_shared<MockSubscription>();

   clientInput_ =
       std::make_shared<MockSubscriber<std::unique_ptr<folly::IOBuf>>>();
   EXPECT_CALL(*clientInput_, onSubscribe_(_))
       .WillOnce(Invoke([&](std::shared_ptr<Subscription> subscription) {
         // allow receiving frames from the automaton
         subscription->request(std::numeric_limits<size_t>::max());
       }));

   clientConnection_->setInput(clientInput_);
   clientOutput_ = clientConnection_->getOutput();
   clientOutput_->onSubscribe(testInputSubscription);
  }

  std::unique_ptr<InlineConnection> clientConnection_;
  std::unique_ptr<InlineConnection> serverConnection_;
  std::shared_ptr<MockSubscriber<std::unique_ptr<folly::IOBuf>>> clientInput_;
  std::shared_ptr<Subscriber<std::unique_ptr<folly::IOBuf>>> clientOutput_;
  std::shared_ptr<MockConnectionHandler> handler_;
  ServerConnectionAcceptor acceptor_;
};

TEST_F(ServerConnectionAcceptorTest, BrokenFrame) {
  EXPECT_CALL(*handler_, connectionError(_,_));
  EXPECT_CALL(*clientInput_, onError_(_));

  acceptor_.accept(std::move(serverConnection_), handler_);
  clientOutput_->onNext(folly::IOBuf::create(0));
  clientOutput_->onComplete();
}

TEST_F(ServerConnectionAcceptorTest, EarlyDisconnect) {
  EXPECT_CALL(*handler_, connectionError(_,_));
  EXPECT_CALL(*clientInput_, onComplete_());

  acceptor_.accept(std::move(serverConnection_), handler_);
  clientOutput_->onComplete();
}

TEST_F(ServerConnectionAcceptorTest, EarlyError) {
  EXPECT_CALL(*handler_, connectionError(_,_));
  EXPECT_CALL(*clientInput_, onError_(_));

  acceptor_.accept(std::move(serverConnection_), handler_);
  clientOutput_->onError(std::runtime_error("need to go"));
}

TEST_F(ServerConnectionAcceptorTest, SetupFrame) {
  ConnectionSetupPayload setupPayload(
    "metadataMimeType",
    "dataMimeType",
    Payload(),
    true);
  EXPECT_CALL(*handler_, doSetupNewSocket(_, _))
    .WillOnce(Invoke([&](
      std::shared_ptr<FrameTransport> transport,
      ConnectionSetupPayload& payload) {
        ASSERT_EQ(setupPayload.token, payload.token);
        ASSERT_EQ(setupPayload.metadataMimeType, payload.metadataMimeType);
        ASSERT_EQ(setupPayload.dataMimeType, payload.dataMimeType);
        transport->close(folly::exception_wrapper());
    }));

  auto frameSerializer = FrameSerializer::createCurrentVersion();
  acceptor_.accept(std::move(serverConnection_), handler_);
  clientOutput_->onNext(frameSerializer->serializeOut(Frame_SETUP(
    setupPayload.resumable ? FrameFlags::RESUME_ENABLE : FrameFlags::EMPTY,
    FrameSerializer::getCurrentProtocolVersion().major,
    FrameSerializer::getCurrentProtocolVersion().minor,
    500,
    Frame_SETUP::kMaxLifetime,
    setupPayload.token,
    setupPayload.metadataMimeType,
    setupPayload.dataMimeType,
    Payload())));
  clientOutput_->onNext(frameSerializer->serializeOut(Frame_REQUEST_FNF(
    1,
    FrameFlags::EMPTY,
    Payload())));
  clientOutput_->onComplete();
}

TEST_F(ServerConnectionAcceptorTest, ResumeFrame) {
  ResumeParameters resumeParams(
      ResumeIdentificationToken::generateNew(),
      1,
      2,
      FrameSerializer::getCurrentProtocolVersion());
  EXPECT_CALL(*handler_, resumeSocket(_, _))
    .WillOnce(Invoke([&](
      std::shared_ptr<FrameTransport> transport,
      ResumeParameters params) -> bool {
        EXPECT_EQ(resumeParams.token, params.token);
        // FIXME: This isn't sent in 0.1 frames
        // ASSERT_EQ(resumeParams.clientPosition, params.clientPosition);
        EXPECT_EQ(resumeParams.serverPosition, params.serverPosition);
        transport->close(folly::exception_wrapper());
        return true;
    }));

  auto frameSerializer = FrameSerializer::createCurrentVersion();
  acceptor_.accept(std::move(serverConnection_), handler_);
  clientOutput_->onNext(frameSerializer->serializeOut(Frame_RESUME(
      resumeParams.token,
      resumeParams.serverPosition,
      resumeParams.clientPosition,
      FrameSerializer::getCurrentProtocolVersion())));
  clientOutput_->onComplete();
}
