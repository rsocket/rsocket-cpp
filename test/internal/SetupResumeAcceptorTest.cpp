// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include <folly/io/async/EventBase.h>

#include "rsocket/framing/FrameTransport.h"
#include "rsocket/internal/SetupResumeAcceptor.h"
#include "test/test_utils/MockDuplexConnection.h"
#include "test/test_utils/MockFrameProcessor.h"
#include "test/test_utils/Mocks.h"

using namespace rsocket;
using namespace testing;

namespace {

/*
 * Make a legitimate-looking SETUP frame.
 */
Frame_SETUP makeSetup() {
  Frame_SETUP frame;
  frame.header_ = FrameHeader{FrameType::SETUP, FrameFlags::EMPTY, 0};
  frame.versionMajor_ = 1;
  frame.versionMinor_ = 0;
  frame.keepaliveTime_ = Frame_SETUP::kMaxKeepaliveTime;
  frame.maxLifetime_ = Frame_SETUP::kMaxLifetime;
  frame.token_ = ResumeIdentificationToken::generateNew();
  frame.metadataMimeType_ = "application/olive+oil";
  frame.dataMimeType_ = "json/vorhees";
  frame.payload_ = Payload("Test SETUP data", "Test SETUP metadata");
  return frame;
}

/*
 * Make a legitimate-looking RESUME frame.
 */
Frame_RESUME makeResume() {
  Frame_RESUME frame;
  frame.header_ = FrameHeader{FrameType::RESUME, FrameFlags::EMPTY, 0};
  frame.versionMajor_ = 1;
  frame.versionMinor_ = 0;
  frame.token_ = ResumeIdentificationToken::generateNew();
  frame.lastReceivedServerPosition_ = 500;
  frame.clientPosition_ = 300;
  return frame;
}

void setupFail(yarpl::Reference<FrameTransport> transport, SetupParameters) {
  transport->close();
  FAIL() << "setupFail() was called";
}

bool resumeFail(yarpl::Reference<FrameTransport> transport, ResumeParameters) {
  transport->close();
  ADD_FAILURE() << "resumeFail() was called";
  return false;
}
}

TEST(SetupResumeAcceptor, ImmediateDtor) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor1{ProtocolVersion::Latest, &evb};
  SetupResumeAcceptor acceptor2{ProtocolVersion::Unknown, &evb};
}

TEST(SetupResumeAcceptor, ImmediateClose) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor1{ProtocolVersion::Latest, &evb};
  SetupResumeAcceptor acceptor2{ProtocolVersion::Unknown, &evb};
  acceptor1.close().get();
  acceptor2.close().get();
}


TEST(SetupResumeAcceptor, EarlyComplete) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        input->onComplete();
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onComplete_());
      });

  acceptor.accept(std::move(connection), setupFail, resumeFail);

  evb.loop();
}

TEST(SetupResumeAcceptor, EarlyError) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        input->onError(std::runtime_error("Whoops"));
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onError_(_));
      });

  acceptor.accept(std::move(connection), setupFail, resumeFail);

  evb.loop();
}

TEST(SetupResumeAcceptor, SingleSetup) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        auto serializer = FrameSerializer::createCurrentVersion();
        input->onNext(serializer->serializeOut(makeSetup()));
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onComplete_());
      });

  bool setupCalled = false;

  acceptor.accept(
      std::move(connection),
      [&](auto transport, auto) {
        transport->close();
        setupCalled = true;
      },
      resumeFail);

  evb.loop();

  EXPECT_TRUE(setupCalled);
}

TEST(SetupResumeAcceptor, SetupAndFnf) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        auto serializer = FrameSerializer::createCurrentVersion();

        auto setup = makeSetup();
        Frame_REQUEST_FNF fnf{100, FrameFlags::EMPTY, Payload("Hi")};

        input->onNext(serializer->serializeOut(std::move(setup)));
        input->onNext(serializer->serializeOut(std::move(fnf)));
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onComplete_());
      });

  yarpl::Reference<FrameTransport> transport;

  acceptor.accept(
      std::move(connection),
      [&](auto tport, auto) { transport = std::move(tport); },
      resumeFail);

  evb.loop();

  EXPECT_TRUE(transport.get());

  auto processor = std::make_shared<StrictMock<MockFrameProcessor>>();
  EXPECT_CALL(*processor, processFrame_(_))
    .WillOnce(Invoke([](auto const& buf) {
          auto serializer = FrameSerializer::createCurrentVersion();

          Frame_REQUEST_FNF fnf;
          EXPECT_TRUE(serializer->deserializeFrom(fnf, buf->clone()));
          EXPECT_EQ(fnf.header_.streamId_, 100u);
          EXPECT_EQ(fnf.header_.flags_, FrameFlags::EMPTY);
          EXPECT_EQ(fnf.payload_.cloneDataToString(), "Hi");
        }));
  transport->setFrameProcessor(processor);
  transport->close();
}

TEST(SetupResumeAcceptor, InvalidSetup) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        auto serializer = FrameSerializer::createCurrentVersion();

        // Bogus keepalive time that can't be deserialized.
        auto setup = makeSetup();
        setup.keepaliveTime_ = -5;

        input->onNext(serializer->serializeOut(std::move(setup)));
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onNext_(_)).WillOnce(Invoke([](auto const& buf) {
          auto serializer = FrameSerializer::createCurrentVersion();
          Frame_ERROR frame;
          EXPECT_TRUE(serializer->deserializeFrom(frame, buf->clone()));
          EXPECT_EQ(frame.errorCode_, ErrorCode::CONNECTION_ERROR);
        }));
        EXPECT_CALL(*output, onError_(_));
      });

  acceptor.accept(std::move(connection), setupFail, resumeFail);

  evb.loop();
}

TEST(SetupResumeAcceptor, RejectedSetup) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        auto serializer = FrameSerializer::createCurrentVersion();
        input->onNext(serializer->serializeOut(makeSetup()));
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onNext_(_)).WillOnce(Invoke([](auto const& buf) {
          auto serializer = FrameSerializer::createCurrentVersion();
          Frame_ERROR frame;
          EXPECT_TRUE(serializer->deserializeFrom(frame, buf->clone()));
          EXPECT_EQ(frame.errorCode_, ErrorCode::REJECTED_SETUP);
        }));
        EXPECT_CALL(*output, onError_(_));
      });

  bool setupCalled = false;

  acceptor.accept(
      std::move(connection),
      [&](auto, auto) {
        setupCalled = true;
        throw std::runtime_error("Oops");
      },
      resumeFail);

  evb.loop();

  EXPECT_TRUE(setupCalled);
}

TEST(SetupResumeAcceptor, RejectedResume) {
  folly::EventBase evb;
  SetupResumeAcceptor acceptor{ProtocolVersion::Latest, &evb};

  auto connection = std::make_unique<StrictMock<MockDuplexConnection>>(
      [](auto input) {
        auto serializer = FrameSerializer::createCurrentVersion();
        input->onNext(serializer->serializeOut(makeResume()));
      },
      [](auto output) {
        EXPECT_CALL(*output, onSubscribe_(_));
        EXPECT_CALL(*output, onNext_(_)).WillOnce(Invoke([](auto const& buf) {
          auto serializer = FrameSerializer::createCurrentVersion();
          Frame_ERROR frame;
          EXPECT_TRUE(serializer->deserializeFrom(frame, buf->clone()));
          EXPECT_EQ(frame.errorCode_, ErrorCode::REJECTED_RESUME);
        }));
        EXPECT_CALL(*output, onError_(_));
      });

  bool resumeCalled = false;

  acceptor.accept(std::move(connection), setupFail, [&](auto, auto) {
    resumeCalled = true;
    throw std::runtime_error("Cant resume");
  });

  evb.loop();

  EXPECT_TRUE(resumeCalled);
}

TEST(SetupResumeAcceptor, EventBaseDisappear) {
  auto evb = std::make_unique<folly::EventBase>();

  SetupResumeAcceptor acceptor{
      ProtocolVersion::Latest, evb.get(), std::this_thread::get_id()};
  evb.reset();
}

// TODO: Test for whether changing FrameProcessor in on{Resume,Setup} breaks
// things.
