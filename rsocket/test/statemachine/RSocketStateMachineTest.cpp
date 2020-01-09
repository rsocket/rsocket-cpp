// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "rsocket/statemachine/RSocketStateMachine.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <yarpl/single/SingleSubscriptions.h>
#include <yarpl/single/Singles.h>
#include <yarpl/test_utils/Mocks.h>
#include "rsocket/RSocketConnectionEvents.h"
#include "rsocket/RSocketResponder.h"
#include "rsocket/framing/FrameSerializer_v1_0.h"
#include "rsocket/framing/FrameTransportImpl.h"
#include "rsocket/internal/Common.h"
#include "rsocket/statemachine/ChannelRequester.h"
#include "rsocket/statemachine/ChannelResponder.h"
#include "rsocket/statemachine/RequestResponseResponder.h"
#include "rsocket/test/test_utils/MockDuplexConnection.h"
#include "rsocket/test/test_utils/MockStreamsWriter.h"

using namespace testing;
using namespace yarpl::mocks;
using namespace yarpl::single;

namespace rsocket {

class ResponderMock : public RSocketResponder {
 public:
  MOCK_METHOD1(
      handleRequestResponse_,
      std::shared_ptr<Single<Payload>>(StreamId));
  MOCK_METHOD1(
      handleRequestStream_,
      std::shared_ptr<yarpl::flowable::Flowable<Payload>>(StreamId));
  MOCK_METHOD2(
      handleRequestChannel_,
      std::shared_ptr<yarpl::flowable::Flowable<Payload>>(
          std::shared_ptr<yarpl::flowable::Flowable<Payload>> requestStream,
          StreamId streamId));

  std::shared_ptr<Single<Payload>> handleRequestResponse(Payload, StreamId id)
      override {
    return handleRequestResponse_(id);
  }

  std::shared_ptr<yarpl::flowable::Flowable<Payload>> handleRequestStream(
      Payload,
      StreamId id) override {
    return handleRequestStream_(id);
  }

  std::shared_ptr<yarpl::flowable::Flowable<Payload>> handleRequestChannel(
      Payload,
      std::shared_ptr<yarpl::flowable::Flowable<Payload>> requestStream,
      StreamId streamId) override {
    return handleRequestChannel_(requestStream, streamId);
  }
};

struct ConnectionEventsMock : public RSocketConnectionEvents {
  MOCK_METHOD1(onDisconnected, void(const folly::exception_wrapper&));
  MOCK_METHOD0(onStreamsPaused, void());
};

class RSocketStateMachineTest : public Test {
 public:
  auto createClient(
      std::unique_ptr<MockDuplexConnection> connection,
      std::shared_ptr<RSocketResponder> responder) {
    EXPECT_CALL(*connection, setInput_(_));
    EXPECT_CALL(*connection, isFramed());

    auto transport =
        std::make_shared<FrameTransportImpl>(std::move(connection));

    auto stateMachine = std::make_shared<RSocketStateMachine>(
        std::move(responder),
        nullptr,
        RSocketMode::CLIENT,
        nullptr,
        nullptr,
        ResumeManager::makeEmpty(),
        nullptr);

    SetupParameters setupParameters;
    setupParameters.resumable = false; // Not resumable!
    stateMachine->connectClient(
        std::move(transport), std::move(setupParameters));

    return stateMachine;
  }

  auto createServer(
      std::unique_ptr<MockDuplexConnection> connection,
      std::shared_ptr<RSocketResponder> responder,
      folly::Optional<ResumeIdentificationToken> resumeToken = folly::none,
      std::shared_ptr<RSocketConnectionEvents> connectionEvents = nullptr) {
    auto transport =
        std::make_shared<FrameTransportImpl>(std::move(connection));

    auto stateMachine = std::make_shared<RSocketStateMachine>(
        std::move(responder),
        nullptr,
        RSocketMode::SERVER,
        nullptr,
        std::move(connectionEvents),
        ResumeManager::makeEmpty(),
        nullptr);

    if (resumeToken) {
      SetupParameters setupParameters;
      setupParameters.resumable = true;
      setupParameters.token = *resumeToken;
      stateMachine->connectServer(std::move(transport), setupParameters);
    } else {
      SetupParameters setupParameters;
      setupParameters.resumable = false;
      stateMachine->connectServer(std::move(transport), setupParameters);
    }

    return stateMachine;
  }

  const std::unordered_map<StreamId, std::shared_ptr<StreamStateMachineBase>>&
  getStreams(RSocketStateMachine& stateMachine) {
    return stateMachine.streams_;
  }

  void setupRequestStream(
      RSocketStateMachine& stateMachine,
      StreamId streamId,
      uint32_t requestN,
      Payload payload) {
    stateMachine.onRequestStreamFrame(
        streamId, requestN, std::move(payload), false);
  }

  void setupRequestChannel(
      RSocketStateMachine& stateMachine,
      StreamId streamId,
      uint32_t requestN,
      Payload payload) {
    stateMachine.onRequestChannelFrame(
        streamId, requestN, std::move(payload), false, true, false);
  }

  void setupRequestResponse(
      RSocketStateMachine& stateMachine,
      StreamId streamId,
      Payload payload) {
    stateMachine.onRequestResponseFrame(streamId, std::move(payload), false);
  }

  void setupFireAndForget(
      RSocketStateMachine& stateMachine,
      StreamId streamId,
      Payload payload) {
    stateMachine.onFireAndForgetFrame(streamId, std::move(payload), false);
  }
};

TEST_F(RSocketStateMachineTest, RequestStream) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  // Setup frame and request stream frame
  EXPECT_CALL(*connection, send_(_)).Times(2);

  auto stateMachine =
      createClient(std::move(connection), std::make_shared<RSocketResponder>());

  auto subscriber = std::make_shared<StrictMock<MockSubscriber<Payload>>>(1000);
  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onComplete_());

  stateMachine->requestStream(Payload{}, subscriber);

  auto& streams = getStreams(*stateMachine);
  ASSERT_EQ(1, streams.size());

  // This line causes: subscriber.onComplete()
  streams.at(1)->endStream(StreamCompletionSignal::CANCEL);

  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, RequestStream_EarlyClose) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  // Setup frame, two request stream frames, one extra frame
  EXPECT_CALL(*connection, send_(_)).Times(3);

  auto stateMachine =
      createClient(std::move(connection), std::make_shared<RSocketResponder>());

  auto subscriber = std::make_shared<StrictMock<MockSubscriber<Payload>>>(1000);
  EXPECT_CALL(*subscriber, onSubscribe_(_)).Times(2);
  EXPECT_CALL(*subscriber, onComplete_());

  stateMachine->requestStream(Payload{}, subscriber);

  // Second stream
  stateMachine->requestStream(Payload{}, subscriber);

  auto& streams = getStreams(*stateMachine);
  ASSERT_EQ(2, streams.size());

  // Close the stream
  auto writer = std::dynamic_pointer_cast<StreamsWriter>(stateMachine);
  writer->onStreamClosed(1);

  // Push more data to the closed stream
  auto processor = std::dynamic_pointer_cast<FrameProcessor>(stateMachine);
  FrameSerializerV1_0 serializer;
  processor->processFrame(
      serializer.serializeOut(Frame_PAYLOAD(1, FrameFlags::COMPLETE, {})));

  // Second stream should still be valid
  ASSERT_EQ(1, streams.size());

  streams.at(3)->endStream(StreamCompletionSignal::CANCEL);
  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, RequestChannel) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  // Setup frame and request channel frame
  EXPECT_CALL(*connection, send_(_)).Times(2);

  auto stateMachine =
      createClient(std::move(connection), std::make_shared<RSocketResponder>());

  auto in = std::make_shared<StrictMock<MockSubscriber<Payload>>>(1000);
  EXPECT_CALL(*in, onSubscribe_(_));
  EXPECT_CALL(*in, onComplete_());

  auto out = stateMachine->requestChannel(Payload{}, true, in);
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, cancel_());
  out->onSubscribe(subscription);

  auto& streams = getStreams(*stateMachine);
  ASSERT_EQ(1, streams.size());

  // This line causes: in.onComplete() and outSubscription.cancel()
  streams.at(1)->endStream(StreamCompletionSignal::CANCEL);

  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, RequestResponse) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  // Setup frame and request channel frame
  EXPECT_CALL(*connection, send_(_)).Times(2);

  auto stateMachine =
      createClient(std::move(connection), std::make_shared<RSocketResponder>());

  auto in = std::make_shared<SingleObserverBase<Payload>>();
  stateMachine->requestResponse(Payload{}, in);

  auto& streams = getStreams(*stateMachine);
  ASSERT_EQ(1, streams.size());

  // This line closes the stream
  streams.at(1)->handlePayload(Payload{"test", "123"}, true, false, false);

  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, RespondStream) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  int requestCount = 5;
  // Payload frames plus a SETUP frame and an ERROR frame
  EXPECT_CALL(*connection, send_(_)).Times(requestCount + 2);

  int sendCount = 0;
  auto responder = std::make_shared<StrictMock<ResponderMock>>();
  EXPECT_CALL(*responder, handleRequestStream_(_))
      .WillOnce(Return(
          yarpl::flowable::Flowable<Payload>::fromGenerator([&sendCount]() {
            ++sendCount;
            return Payload{};
          })));

  auto stateMachine = createClient(std::move(connection), responder);
  setupRequestStream(*stateMachine, 2, requestCount, Payload{});
  EXPECT_EQ(requestCount, sendCount);

  auto& streams = getStreams(*stateMachine);
  EXPECT_EQ(1, streams.size());

  // releases connection and the responder
  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, RespondChannel) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  int requestCount = 5;
  // + the cancel frame when the stateMachine gets destroyed
  EXPECT_CALL(*connection, send_(_)).Times(requestCount + 1);

  int sendCount = 0;
  auto responder = std::make_shared<StrictMock<ResponderMock>>();
  EXPECT_CALL(*responder, handleRequestChannel_(_, _))
      .WillOnce(Return(
          yarpl::flowable::Flowable<Payload>::fromGenerator([&sendCount]() {
            ++sendCount;
            return Payload{};
          })));

  auto stateMachine = createClient(std::move(connection), responder);
  setupRequestChannel(*stateMachine, 2, requestCount, Payload{});
  EXPECT_EQ(requestCount, sendCount);

  auto& streams = getStreams(*stateMachine);
  EXPECT_EQ(1, streams.size());

  // releases connection and the responder
  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, RespondRequest) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  EXPECT_CALL(*connection, send_(_)).Times(2);

  int sendCount = 0;
  auto responder = std::make_shared<StrictMock<ResponderMock>>();
  EXPECT_CALL(*responder, handleRequestResponse_(_))
      .WillOnce(Return(Singles::fromGenerator<Payload>([&sendCount]() {
        ++sendCount;
        return Payload{};
      })));

  auto stateMachine = createClient(std::move(connection), responder);
  setupRequestResponse(*stateMachine, 2, Payload{});
  EXPECT_EQ(sendCount, 1);

  auto& streams = getStreams(*stateMachine);
  EXPECT_EQ(0, streams.size()); // already completed

  // releases connection and the responder
  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, StreamImmediateCancel) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  // Only send a SETUP frame.  A REQUEST_STREAM frame should never be sent.
  EXPECT_CALL(*connection, send_(_));

  auto stateMachine =
      createClient(std::move(connection), std::make_shared<RSocketResponder>());

  auto subscriber = std::make_shared<StrictMock<MockSubscriber<Payload>>>();
  EXPECT_CALL(*subscriber, onSubscribe_(_))
      .WillOnce(Invoke(
          [](std::shared_ptr<yarpl::flowable::Subscription> subscription) {
            subscription->cancel();
          }));

  stateMachine->requestStream(Payload{}, subscriber);

  auto& streams = getStreams(*stateMachine);
  ASSERT_EQ(0, streams.size());

  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

TEST_F(RSocketStateMachineTest, TransportOnNextClose) {
  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>();
  // Only SETUP frame gets sent.
  EXPECT_CALL(*connection, setInput_(_));
  EXPECT_CALL(*connection, isFramed());
  EXPECT_CALL(*connection, send_(_));

  auto transport = std::make_shared<FrameTransportImpl>(std::move(connection));
  auto stateMachine = std::make_shared<RSocketStateMachine>(
      std::make_shared<StrictMock<ResponderMock>>(),
      nullptr,
      RSocketMode::CLIENT,
      nullptr,
      nullptr,
      ResumeManager::makeEmpty(),
      nullptr);

  SetupParameters params;
  params.resumable = false;
  stateMachine->connectClient(transport, std::move(params));

  auto rawTransport = transport.get();

  // Leak the cycle.
  stateMachine.reset();
  transport.reset();

  FrameSerializerV1_0 serializer;
  auto buf = serializer.serializeOut(Frame_ERROR::connectionError("Hah!"));
  rawTransport->onNext(std::move(buf));
}

TEST_F(RSocketStateMachineTest, ResumeWithCurrentConnection) {
  auto resumeToken = ResumeIdentificationToken::generateNew();

  auto eventsMock = std::make_shared<ConnectionEventsMock>();
  auto stateMachine = createServer(
      std::make_unique<NiceMock<MockDuplexConnection>>(),
      std::make_shared<RSocketResponder>(),
      resumeToken,
      eventsMock);

  EXPECT_CALL(*eventsMock, onDisconnected(_)).Times(0);
  EXPECT_CALL(*eventsMock, onStreamsPaused()).Times(0);

  ResumeParameters resumeParams{resumeToken, 0, 0, ProtocolVersion::Latest};
  auto transport = std::make_shared<FrameTransportImpl>(
      std::make_unique<NiceMock<MockDuplexConnection>>());
  stateMachine->resumeServer(transport, resumeParams);

  stateMachine->close({}, StreamCompletionSignal::CONNECTION_END);
}

} // namespace rsocket
