// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/StandardReactiveSocket.h"

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
#include "src/ClientResumeStatusCallback.h"
#include "src/ConnectionAutomaton.h"
#include "src/FrameTransport.h"
#include "src/ReactiveSocket.h"
#include "src/automata/ChannelRequester.h"
#include "src/automata/ChannelResponder.h"
#include "src/automata/RequestResponseRequester.h"
#include "src/automata/RequestResponseResponder.h"
#include "src/automata/StreamRequester.h"
#include "src/automata/StreamResponder.h"
#include "src/automata/SubscriptionRequester.h"
#include "src/automata/SubscriptionResponder.h"

namespace reactivesocket {

StandardReactiveSocket::~StandardReactiveSocket() {
  // Force connection closure, this will trigger terminal signals to be
  // delivered to all stream automata.
  close();

  // no more callbacks after the destructor is executed
  onConnectListeners_->clear();
  onDisconnectListeners_->clear();
  onCloseListeners_->clear();
}

StandardReactiveSocket::StandardReactiveSocket(
    bool isServer,
    std::shared_ptr<RequestHandler> handler,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    folly::Executor& executor)
    : handler_(handler),
      connection_(std::make_shared<ConnectionAutomaton>(
          [this, handler](
              ConnectionAutomaton& connection,
              StreamId streamId,
              std::unique_ptr<folly::IOBuf> serializedFrame) {
            createResponder(
                handler, connection, streamId, std::move(serializedFrame));
          },
          std::make_shared<StreamState>(),
          std::bind(
              &StandardReactiveSocket::resumeListener,
              this,
              std::placeholders::_1),
          stats,
          std::move(keepaliveTimer),
          isServer,
          executeListenersFunc(onConnectListeners_),
          executeListenersFunc(onDisconnectListeners_),
          executeListenersFunc(onCloseListeners_))),
      nextStreamId_(isServer ? 1 : 2),
      executor_(executor) {
  stats.socketCreated();
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::fromClientConnection(
    folly::Executor& executor,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    ConnectionSetupPayload setupPayload,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer) {
  auto socket = disconnectedClient(
      executor, std::move(handler), stats, std::move(keepaliveTimer));
  socket->clientConnect(
      std::make_shared<FrameTransport>(std::move(connection)),
      std::move(setupPayload));
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::disconnectedClient(
    folly::Executor& executor,
    std::unique_ptr<RequestHandler> handler,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer) {
  std::unique_ptr<StandardReactiveSocket> socket(new StandardReactiveSocket(
      false, std::move(handler), stats, std::move(keepaliveTimer), executor));
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::fromServerConnection(
    folly::Executor& executor,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    Stats& stats,
    bool isResumable) {
  // TODO: isResumable should come as a flag on Setup frame and it should be
  // exposed to the application code. We should then remove this parameter
  auto socket = disconnectedServer(executor, std::move(handler), stats);
  socket->serverConnect(
      std::make_shared<FrameTransport>(std::move(connection)), isResumable);
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::disconnectedServer(
    folly::Executor& executor,
    std::unique_ptr<RequestHandler> handler,
    Stats& stats) {
  std::unique_ptr<StandardReactiveSocket> socket(new StandardReactiveSocket(
      true, std::move(handler), stats, nullptr, executor));
  return socket;
}

std::shared_ptr<Subscriber<Payload>> StandardReactiveSocket::requestChannel(
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  checkNotClosed();
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  ChannelRequester::Parameters params = {{connection_, streamId, handler_},
                                         executor_};
  auto automaton = std::make_shared<ChannelRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(std::move(responseSink));
  automaton->start();
  return automaton;
}

void StandardReactiveSocket::requestStream(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  checkNotClosed();
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  StreamRequester::Parameters params = {{connection_, streamId, handler_},
                                        executor_};
  auto automaton = std::make_shared<StreamRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(std::move(responseSink));
  automaton->processInitialPayload(std::move(request));
  automaton->start();
}

void StandardReactiveSocket::requestSubscription(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  checkNotClosed();
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  SubscriptionRequester::Parameters params = {{connection_, streamId, handler_},
                                              executor_};
  auto automaton = std::make_shared<SubscriptionRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(std::move(responseSink));
  automaton->processInitialPayload(std::move(request));
  automaton->start();
}

void StandardReactiveSocket::requestFireAndForget(Payload request) {
  checkNotClosed();
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  Frame_REQUEST_FNF frame(
      streamId, FrameFlags_EMPTY, std::move(std::move(request)));
  connection_->outputFrameOrEnqueue(frame.serializeOut());
}

void StandardReactiveSocket::requestResponse(
    Payload payload,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  checkNotClosed();
  // TODO(stupaq): handle any exceptions
  StreamId streamId = nextStreamId_;
  nextStreamId_ += 2;
  RequestResponseRequester::Parameters params = {
      {connection_, streamId, handler_}, executor_};
  auto automaton = std::make_shared<RequestResponseRequester>(params);
  connection_->addStream(streamId, automaton);
  automaton->subscribe(std::move(responseSink));
  automaton->processInitialPayload(std::move(payload));
  automaton->start();
}

void StandardReactiveSocket::metadataPush(
    std::unique_ptr<folly::IOBuf> metadata) {
  checkNotClosed();
  connection_->outputFrameOrEnqueue(
      Frame_METADATA_PUSH(std::move(metadata)).serializeOut());
}

void StandardReactiveSocket::createResponder(
    std::shared_ptr<RequestHandler> handler,
    ConnectionAutomaton& connection,
    StreamId streamId,
    std::unique_ptr<folly::IOBuf> serializedFrame) {
  auto type = FrameHeader::peekType(*serializedFrame);
  switch (type) {
    case FrameType::SETUP: {
      Frame_SETUP frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      if (frame.header_.flags_ & FrameFlags_LEASE) {
        // TODO(yschimke) We don't have the correct lease and wait logic above
        // yet
        LOG(WARNING) << "ignoring setup frame with lease";
        //          connectionOutput_.onNext(
        //              Frame_ERROR::badSetupFrame("leases not supported")
        //                  .serializeOut());
        //          disconnect();
      }

      auto streamState = handler->handleSetupPayload(ConnectionSetupPayload(
          std::move(frame.metadataMimeType_),
          std::move(frame.dataMimeType_),
          std::move(frame.payload_),
          false, // TODO: resumable flag should be received in SETUP frame
          frame.token_));

      // TODO(lehecka): use again
      // connection.useStreamState(streamState);
      break;
    }
    case FrameType::RESUME: {
      Frame_RESUME frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      handler->handleResume(frame.token_, frame.position_);
      break;
    }
    case FrameType::REQUEST_CHANNEL: {
      Frame_REQUEST_CHANNEL frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }

      ChannelResponder::Parameters params = {
          {connection.shared_from_this(), streamId, handler}, executor_};
      auto automaton = std::make_shared<ChannelResponder>(params);
      connection.addStream(streamId, automaton);

      auto requestSink = handler->handleRequestChannel(
          std::move(frame.payload_), streamId, automaton);
      automaton->subscribe(requestSink);

      // processInitialFrame executes directly, it may cause to call request(n)
      // which may call back and it will be queued after the calls from
      // the onSubscribe method
      automaton->processInitialFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_STREAM: {
      Frame_REQUEST_STREAM frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }

      StreamResponder::Parameters params = {
          {connection.shared_from_this(), streamId, handler}, executor_};
      auto automaton = std::make_shared<StreamResponder>(params);
      connection.addStream(streamId, automaton);
      handler->handleRequestStream(
          std::move(frame.payload_), streamId, automaton);

      automaton->processInitialFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_SUB: {
      Frame_REQUEST_SUB frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }

      SubscriptionResponder::Parameters params = {
          {connection.shared_from_this(), streamId, handler}, executor_};
      auto automaton = std::make_shared<SubscriptionResponder>(params);
      connection.addStream(streamId, automaton);

      handler->handleRequestSubscription(
          std::move(frame.payload_), streamId, automaton);

      automaton->processInitialFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_RESPONSE: {
      Frame_REQUEST_RESPONSE frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }

      RequestResponseResponder::Parameters params = {
          {connection.shared_from_this(), streamId, handler}, executor_};
      auto automaton = std::make_shared<RequestResponseResponder>(params);
      connection.addStream(streamId, automaton);

      handler->handleRequestResponse(
          std::move(frame.payload_), streamId, automaton);

      automaton->processInitialFrame(std::move(frame));
      automaton->start();
      break;
    }
    case FrameType::REQUEST_FNF: {
      Frame_REQUEST_FNF frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      // no stream tracking is necessary
      handler->handleFireAndForgetRequest(std::move(frame.payload_), streamId);
      break;
    }
    case FrameType::METADATA_PUSH: {
      Frame_METADATA_PUSH frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      handler->handleMetadataPush(std::move(frame.metadata_));
      break;
    }
    // Other frames cannot start a stream.

    case FrameType::LEASE:
    case FrameType::KEEPALIVE:
    case FrameType::RESERVED:
    default:
      connection.closeWithError(Frame_ERROR::unexpectedFrame());
  }
}

std::shared_ptr<StreamState> StandardReactiveSocket::resumeListener(
    const ResumeIdentificationToken& token) {
  CHECK(false) << "not implemented";
  // TODO(lehecka)
  return nullptr;
  //  return handler_->handleResume(token);
}

void StandardReactiveSocket::clientConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload) {
  CHECK(frameTransport && !frameTransport->isClosed());
  checkNotClosed();
  connection_->setResumable(setupPayload.resumable);

  // TODO set correct version
  Frame_SETUP frame(
      FrameFlags_EMPTY,
      0,
      connection_->getKeepaliveTime(),
      std::numeric_limits<uint32_t>::max(),
      // TODO: resumability,
      setupPayload.token,
      std::move(setupPayload.metadataMimeType),
      std::move(setupPayload.dataMimeType),
      std::move(setupPayload.payload));

  // TODO: when the server returns back that it doesn't support resumability, we
  // should retry without resumability

  // making sure we send setup frame first
  frameTransport->outputFrameOrEnqueue(frame.serializeOut());
  // then the rest of the cached frames will be sent
  connection_->connect(std::move(frameTransport), true);
}

void StandardReactiveSocket::serverConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    bool isResumable) {
  connection_->setResumable(isResumable);
  connection_->connect(std::move(frameTransport), true);
}

void StandardReactiveSocket::close() {
  if (connection_) {
    connection_->close();
    connection_ = nullptr;
  }
}

void StandardReactiveSocket::disconnect() {
  checkNotClosed();
  connection_->disconnect();
}

std::shared_ptr<FrameTransport> StandardReactiveSocket::detachFrameTransport() {
  checkNotClosed();
  return connection_->detachFrameTransport();
}

void StandardReactiveSocket::onConnected(ReactiveSocketCallback listener) {
  checkNotClosed();
  CHECK(listener);
  onConnectListeners_->push_back(std::move(listener));
}

void StandardReactiveSocket::onDisconnected(ReactiveSocketCallback listener) {
  checkNotClosed();
  CHECK(listener);
  onDisconnectListeners_->push_back(std::move(listener));
}

void StandardReactiveSocket::onClosed(ReactiveSocketCallback listener) {
  checkNotClosed();
  CHECK(listener);
  onCloseListeners_->push_back(std::move(listener));
}

void StandardReactiveSocket::tryClientResume(
    const ResumeIdentificationToken& token,
    std::shared_ptr<FrameTransport> frameTransport,
    std::unique_ptr<ClientResumeStatusCallback> resumeCallback) {
  // TODO: verify/assert that the new frameTransport is on the same event base
  checkNotClosed();
  CHECK(frameTransport && !frameTransport->isClosed());

  frameTransport->outputFrameOrEnqueue(
      connection_->createResumeFrame(token).serializeOut());

  connection_->reconnect(std::move(frameTransport), std::move(resumeCallback));
}

bool StandardReactiveSocket::tryResumeServer(
    std::shared_ptr<FrameTransport> frameTransport,
    ResumePosition position) {
  // TODO: verify/assert that the new frameTransport is on the same event base
  checkNotClosed();

  // TODO: verify, we should not be receiving any frames, not a single one
  connection_->connect(std::move(frameTransport), /*sendPendingFrames=*/false);
  return connection_->resumeFromPositionOrClose(position, true);
}

std::function<void()> StandardReactiveSocket::executeListenersFunc(
    std::shared_ptr<std::list<ReactiveSocketCallback>> listeners) {
  auto* thisPtr = this;
  return [thisPtr, listeners]() mutable {
    // we will make a copy of listeners so that destructor won't delete them
    // when iterating them
    auto listenersCopy = *listeners;
    for (auto& listener : listenersCopy) {
      if (listeners->empty()) {
        // destructor deleted listeners
        thisPtr = nullptr;
      }
      // TODO: change parameter from reference to pointer to be able send null
      // when this instance is destroyed in the callback
      listener(*thisPtr);
    }
  };
}

void StandardReactiveSocket::checkNotClosed() const {
  CHECK(connection_) << "ReactiveSocket already closed";
}

DuplexConnection* StandardReactiveSocket::duplexConnection() const {
  return connection_ ? connection_->duplexConnection() : nullptr;
}

} // reactivesocket
