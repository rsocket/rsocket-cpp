// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ReactiveSocket.h"

#include <cassert>
#include <functional>
#include <memory>

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>

#include "src/CancellingSubscription.h"
#include "src/ConnectionAutomaton.h"
#include "src/ConnectionSetupPayload.h"
#include "src/DuplexConnection.h"
#include "src/Frame.h"
#include "src/Payload.h"
#include "src/ReactiveSocketSubscriberFactory.h"
#include "src/RequestHandler.h"
#include "src/automata/ChannelRequester.h"
#include "src/automata/ChannelResponder.h"
#include "src/automata/RequestResponseRequester.h"
#include "src/automata/RequestResponseResponder.h"
#include "src/automata/StreamRequester.h"
#include "src/automata/StreamResponder.h"
#include "src/automata/SubscriptionRequester.h"
#include "src/automata/SubscriptionResponder.h"

namespace reactivesocket {

ReactiveSocket::~ReactiveSocket() {
  // Force connection closure, this will trigger terminal signals to be
  // delivered to all stream automata.
  close();
}

ReactiveSocket::ReactiveSocket(
    bool isServer,
    std::unique_ptr<DuplexConnection> connection,
    std::shared_ptr<RequestHandlerBase> handler,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer)
    : handler_(handler),
      connection_(new ConnectionAutomaton(
          std::move(connection),
          [handler](
              ConnectionAutomaton& connection,
              StreamId streamId,
              std::unique_ptr<folly::IOBuf> serializedFrame) {
            return ReactiveSocket::createResponder(
                *handler, connection, streamId, std::move(serializedFrame));
          },
          std::make_shared<StreamState>(),
          std::bind(
              &ReactiveSocket::resumeListener,
              this,
              std::placeholders::_1),
          stats,
          isServer)),
      nextStreamId_(isServer ? 1 : 2),
      keepaliveTimer_(std::move(keepaliveTimer)) {}

std::unique_ptr<ReactiveSocket> ReactiveSocket::fromClientConnection(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandlerBase> handler,
    ConnectionSetupPayload setupPayload,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    const ResumeIdentificationToken& token) {
  std::unique_ptr<ReactiveSocket> socket(new ReactiveSocket(
      false,
      std::move(connection),
      std::move(handler),
      stats,
      std::move(keepaliveTimer)));
  socket->connection_->connect();

  uint32_t keepaliveTime = socket->keepaliveTimer_
      ? socket->keepaliveTimer_->keepaliveTime().count()
      : std::numeric_limits<uint32_t>::max();

  // TODO set correct version
  Frame_SETUP frame(
      FrameFlags_EMPTY,
      0,
      keepaliveTime,
      std::numeric_limits<uint32_t>::max(),
      token,
      std::move(setupPayload.metadataMimeType),
      std::move(setupPayload.dataMimeType),
      std::move(setupPayload.payload));

  socket->connection_->outputFrameOrEnqueue(frame.serializeOut());

  if (socket->keepaliveTimer_) {
    socket->keepaliveTimer_->start(socket->connection_);
  }

  return socket;
}

std::unique_ptr<ReactiveSocket> ReactiveSocket::fromServerConnection(
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandlerBase> handler,
    Stats& stats) {
  std::unique_ptr<ReactiveSocket> socket(new ReactiveSocket(
      true,
      std::move(connection),
      std::move(handler),
      stats,
      std::unique_ptr<KeepaliveTimer>(nullptr)));
  socket->connection_->connect();
  return socket;
}

std::shared_ptr<Subscriber<Payload>> ReactiveSocket::requestChannel(
    const std::shared_ptr<Subscriber<Payload>>& responseSink,
    folly::Executor& executor) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  ChannelRequester::Parameters params = {{connection_, streamId}, executor};
  auto automaton = std::make_shared<ChannelRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(responseSink);
  responseSink->onSubscribe(automaton);
  automaton->start();
  return automaton;
}

void ReactiveSocket::requestStream(
    Payload request,
    const std::shared_ptr<Subscriber<Payload>>& responseSink,
    folly::Executor& executor) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  StreamRequester::Parameters params = {{connection_, streamId}, executor};
  auto automaton = std::make_shared<StreamRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(responseSink);
  responseSink->onSubscribe(automaton);
  automaton->onNext(std::move(request));
  automaton->start();
}

void ReactiveSocket::requestSubscription(
    Payload request,
    const std::shared_ptr<Subscriber<Payload>>& responseSink,
    folly::Executor& executor) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  SubscriptionRequester::Parameters params = {{connection_, streamId},
                                              executor};
  auto automaton = std::make_shared<SubscriptionRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(responseSink);
  responseSink->onSubscribe(automaton);
  automaton->onNext(std::move(request));
  automaton->start();
}

void ReactiveSocket::requestFireAndForget(Payload request) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  Frame_REQUEST_FNF frame(
      streamId, FrameFlags_EMPTY, std::move(std::move(request)));
  connection_->outputFrameOrEnqueue(frame.serializeOut());
}

void ReactiveSocket::requestResponse(
    Payload payload,
    const std::shared_ptr<Subscriber<Payload>>& responseSink,
    folly::Executor& executor) {
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  RequestResponseRequester::Parameters params = {{connection_, streamId},
                                                 executor};
  auto automaton = std::make_shared<RequestResponseRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(responseSink);
  responseSink->onSubscribe(automaton);
  automaton->onNext(std::move(payload));
  automaton->start();
}

void ReactiveSocket::metadataPush(std::unique_ptr<folly::IOBuf> metadata) {
  connection_->outputFrameOrEnqueue(
      Frame_METADATA_PUSH(std::move(metadata)).serializeOut());
}

bool ReactiveSocket::createResponder(
    RequestHandlerBase& handler,
    ConnectionAutomaton& connection,
    StreamId streamId,
    std::unique_ptr<folly::IOBuf> serializedFrame) {
  auto type = FrameHeader::peekType(*serializedFrame);
  switch (type) {
    case FrameType::SETUP: {
      Frame_SETUP frame;
      if (frame.deserializeFrom(std::move(serializedFrame))) {
        if (frame.header_.flags_ & FrameFlags_LEASE) {
          // TODO(yschimke) We don't have the correct lease and wait logic above
          // yet
          LOG(WARNING) << "ignoring setup frame with lease";
          //          connectionOutput_.onNext(
          //              Frame_ERROR::badSetupFrame("leases not supported")
          //                  .serializeOut());
          //          disconnect();
        }

        auto streamState = handler.handleSetupPayload(ConnectionSetupPayload(
            std::move(frame.metadataMimeType_),
            std::move(frame.dataMimeType_),
            std::move(frame.payload_),
            frame.token_));

        connection.useStreamState(streamState);
      } else {
        // TODO(yschimke) enable this later after clients upgraded
        LOG(WARNING) << "ignoring bad setup frame";
        //        connectionOutput_.onNext(
        //            Frame_ERROR::badSetupFrame("bad setup
        //            frame").serializeOut());
        //        disconnect();
      }

      break;
    }
    case FrameType::REQUEST_CHANNEL: {
      Frame_REQUEST_CHANNEL frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      std::shared_ptr<ChannelResponder> automaton;
      ReactiveSocketSubscriberFactory subscriberFactory(
          [&](folly::Executor* executor) {
            ChannelResponder::Parameters params = {
                {connection.shared_from_this(), streamId},
                executor ? *executor : defaultExecutor()};
            automaton = std::make_shared<ChannelResponder>(params);
            connection.addStream(streamId, automaton);
            return automaton;
          });
      auto requestSink = handler.onRequestChannel(
          std::move(frame.payload_), subscriberFactory);
      if (!automaton) {
        auto subscriber = subscriberFactory.createSubscriber();
        subscriber->onSubscribe(
            std::make_shared<CancellingSubscription>(subscriber));
      }
      if (automaton->subscribe(requestSink)) {
        // any calls from onSubscribe are queued until we start
        // TODO(lehecka): move the onSubscribe call to subscribe method
        requestSink->onSubscribe(automaton);
      }
      // onNextFrame executes directly, it may cause to call request(n)
      // which may call back and it will be queued after the calls from
      // the onSubscribe method
      automaton->onNextFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_STREAM: {
      Frame_REQUEST_STREAM frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      std::shared_ptr<StreamResponder> automaton;
      ReactiveSocketSubscriberFactory subscriberFactory(
          [&](folly::Executor* executor) {
            StreamResponder::Parameters params = {
                {connection.shared_from_this(), streamId},
                executor ? *executor : defaultExecutor()};
            automaton = std::make_shared<StreamResponder>(params);
            connection.addStream(streamId, automaton);
            return automaton;
          });
      handler.onRequestStream(std::move(frame.payload_), subscriberFactory);
      if (!automaton) {
        auto subscriber = subscriberFactory.createSubscriber();
        subscriber->onSubscribe(
            std::make_shared<CancellingSubscription>(subscriber));
      }
      automaton->onNextFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_SUB: {
      Frame_REQUEST_SUB frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      std::shared_ptr<SubscriptionResponder> automaton;
      ReactiveSocketSubscriberFactory subscriberFactory(
          [&](folly::Executor* executor) {
            SubscriptionResponder::Parameters params = {
                {connection.shared_from_this(), streamId},
                executor ? *executor : defaultExecutor()};
            automaton = std::make_shared<SubscriptionResponder>(params);
            connection.addStream(streamId, automaton);
            return automaton;
          });
      handler.onRequestSubscription(
          std::move(frame.payload_), subscriberFactory);
      if (!automaton) {
        auto subscriber = subscriberFactory.createSubscriber();
        subscriber->onSubscribe(
            std::make_shared<CancellingSubscription>(subscriber));
      }
      automaton->onNextFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_RESPONSE: {
      Frame_REQUEST_RESPONSE frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      std::shared_ptr<RequestResponseResponder> automaton;
      ReactiveSocketSubscriberFactory subscriberFactory(
          [&](folly::Executor* executor) {
            RequestResponseResponder::Parameters params = {
                {connection.shared_from_this(), streamId},
                executor ? *executor : defaultExecutor()};
            automaton = std::make_shared<RequestResponseResponder>(params);
            connection.addStream(streamId, automaton);
            return automaton;
          });
      handler.onRequestResponse(std::move(frame.payload_), subscriberFactory);
      // we need to create a responder to at least close the stream
      if (!automaton) {
        auto subscriber = subscriberFactory.createSubscriber();
        subscriber->onSubscribe(
            std::make_shared<CancellingSubscription>(subscriber));
      }
      automaton->onNextFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_FNF: {
      Frame_REQUEST_FNF frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      // no stream tracking is necessary
      handler.handleFireAndForgetRequest(std::move(frame.payload_));
      break;
    }
    case FrameType::METADATA_PUSH: {
      Frame_METADATA_PUSH frame;
      if (!frame.deserializeFrom(std::move(serializedFrame))) {
        return false;
      }
      handler.handleMetadataPush(std::move(frame.metadata_));
      break;
    }
    // Other frames cannot start a stream.
    default:
      return false;
  }
  return true;
}

std::shared_ptr<StreamState> ReactiveSocket::resumeListener(
    const ResumeIdentificationToken& token) {
  return handler_->handleResume(token);
}

void ReactiveSocket::close() {
  // Stop scheduling keepalives since the socket is now closed, but may be destructed later
  if (keepaliveTimer_) {
    keepaliveTimer_->stop();
  }

  connection_->disconnect();
}

void ReactiveSocket::onClose(CloseListener listener) {
  connection_->onClose([listener, this]() { listener(*this); });
}

void ReactiveSocket::tryClientResume(
    std::unique_ptr<DuplexConnection> newConnection,
    const ResumeIdentificationToken& token) {
  connection_->reconnect(std::move(newConnection));
  connection_->sendResume(token);
}

}
