// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/StandardReactiveSocket.h"

#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
#include <folly/io/async/EventBase.h>
#include "src/ClientResumeStatusCallback.h"
#include "src/ConnectionAutomaton.h"
#include "src/FrameTransport.h"
#include "src/RequestHandler.h"
#include "src/automata/ChannelResponder.h"

namespace reactivesocket {

StandardReactiveSocket::~StandardReactiveSocket() {
  debugCheckCorrectExecutor();

  // Force connection closure, this will trigger terminal signals to be
  // delivered to all stream automata.
  close();

  // no more callbacks after the destructor is executed
  onConnectListeners_->clear();
  onDisconnectListeners_->clear();
  onCloseListeners_->clear();
}

StandardReactiveSocket::StandardReactiveSocket(
    ReactiveSocketMode mode,
    std::shared_ptr<RequestHandler> handler,
    Stats& stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    folly::Executor& executor)
    : handler_(handler),
      connection_(std::make_shared<ConnectionAutomaton>(
          executor,
          [this, handler](
              ConnectionAutomaton& connection,
              StreamId streamId,
              std::unique_ptr<folly::IOBuf> serializedFrame) {
            createResponder(
                handler, connection, streamId, std::move(serializedFrame));
          },
          std::make_shared<StreamState>(stats),
          handler,
          std::bind(
              &StandardReactiveSocket::resumeListener,
              this,
              std::placeholders::_1),
          stats,
          std::move(keepaliveTimer),
          mode,
          executeListenersFunc(onConnectListeners_),
          executeListenersFunc(onDisconnectListeners_),
          executeListenersFunc(onCloseListeners_))),
      streamsFactory_(connection_, mode),
      executor_(executor) {
  debugCheckCorrectExecutor();
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
      ReactiveSocketMode::CLIENT,
      std::move(handler),
      stats,
      std::move(keepaliveTimer),
      executor));
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
      ReactiveSocketMode::SERVER,
      std::move(handler),
      stats,
      nullptr,
      executor));
  return socket;
}

std::shared_ptr<Subscriber<Payload>> StandardReactiveSocket::requestChannel(
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  return streamsFactory_.createChannelRequester(
      std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestStream(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  streamsFactory_.createStreamRequester(
      std::move(request), std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestSubscription(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  streamsFactory_.createSubscriptionRequester(
      std::move(request), std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestResponse(
    Payload payload,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  streamsFactory_.createRequestResponseRequester(
      std::move(payload), std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestFireAndForget(Payload request) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  Frame_REQUEST_FNF frame(
      streamsFactory_.getNextStreamId(),
      FrameFlags_EMPTY,
      std::move(std::move(request)));
  connection_->outputFrameOrEnqueue(frame.serializeOut());
}

void StandardReactiveSocket::metadataPush(
    std::unique_ptr<folly::IOBuf> metadata) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->outputFrameOrEnqueue(
      Frame_METADATA_PUSH(std::move(metadata)).serializeOut());
}

void StandardReactiveSocket::createResponder(
    std::shared_ptr<RequestHandler> handler,
    ConnectionAutomaton& connection,
    StreamId streamId,
    std::unique_ptr<folly::IOBuf> serializedFrame) {
  debugCheckCorrectExecutor();

  if (streamId != 0 && !streamsFactory_.registerNewPeerStreamId(streamId)) {
    return;
  }

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

      auto streamState = handler->handleSetupPayload(
          *this,
          ConnectionSetupPayload(
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
      auto resumed =
          handler->handleResume(*this, frame.token_, frame.position_);
      if (!resumed) {
        // TODO(lehecka): the "connection" and "this" arguments needs to be
        // cleaned up. It is not intuitive what is their lifetime.
        auto connectionCopy = std::move(connection_);
        connection.closeWithError(
            Frame_ERROR::connectionError("can not resume"));
      }
      break;
    }
    case FrameType::REQUEST_CHANNEL: {
      Frame_REQUEST_CHANNEL frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      auto automaton = streamsFactory_.createChannelResponder(
          frame.requestN_, streamId, executor_);
      auto requestSink = handler->handleRequestChannel(
          std::move(frame.payload_), streamId, automaton);
      automaton->subscribe(requestSink);
      break;
    }
    case FrameType::REQUEST_STREAM: {
      Frame_REQUEST_STREAM frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      auto automaton = streamsFactory_.createStreamResponder(
          frame.requestN_, streamId, executor_);
      handler->handleRequestStream(
          std::move(frame.payload_), streamId, automaton);
      break;
    }
    case FrameType::REQUEST_SUB: {
      Frame_REQUEST_SUB frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      auto automaton = streamsFactory_.createSubscriptionResponder(
          frame.requestN_, streamId, executor_);
      handler->handleRequestSubscription(
          std::move(frame.payload_), streamId, automaton);
      break;
    }
    case FrameType::REQUEST_RESPONSE: {
      Frame_REQUEST_RESPONSE frame;
      if (!connection.deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      auto automaton =
          streamsFactory_.createRequestResponseResponder(streamId, executor_);
      handler->handleRequestResponse(
          std::move(frame.payload_), streamId, automaton);
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
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::RESPONSE:
    case FrameType::ERROR:
    case FrameType::RESUME_OK:
    default:
      // TODO(lehecka): the "connection" and "this" arguments needs to be
      // cleaned up. It is not intuitive what is their lifetime.
      auto connectionCopy = std::move(connection_);
      connection.closeWithError(Frame_ERROR::unexpectedFrame());
  }
}

std::shared_ptr<StreamState> StandardReactiveSocket::resumeListener(
    const ResumeIdentificationToken& token) {
  debugCheckCorrectExecutor();
  CHECK(false) << "not implemented";
  // TODO(lehecka)
  return nullptr;
  //  return handler_->handleResume(token);
}

void StandardReactiveSocket::clientConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload) {
  CHECK(frameTransport && !frameTransport->isClosed());
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->setResumable(setupPayload.resumable);

  // TODO set correct version
  Frame_SETUP frame(
      setupPayload.resumable ? FrameFlags_RESUME_ENABLE : FrameFlags_EMPTY,
      /*version=*/0,
      connection_->getKeepaliveTime(),
      std::numeric_limits<uint32_t>::max(),
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
  debugCheckCorrectExecutor();
  connection_->setResumable(isResumable);
  connection_->connect(std::move(frameTransport), true);
}

void StandardReactiveSocket::close() {
  debugCheckCorrectExecutor();
  if (auto connectionCopy = std::move(connection_)) {
    connectionCopy->close();
  }
}

void StandardReactiveSocket::disconnect() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->disconnect();
}

std::shared_ptr<FrameTransport> StandardReactiveSocket::detachFrameTransport() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  return connection_->detachFrameTransport();
}

void StandardReactiveSocket::onConnected(ReactiveSocketCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  CHECK(listener);
  onConnectListeners_->push_back(std::move(listener));
}

void StandardReactiveSocket::onDisconnected(ReactiveSocketCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  CHECK(listener);
  onDisconnectListeners_->push_back(std::move(listener));
}

void StandardReactiveSocket::onClosed(ReactiveSocketCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  CHECK(listener);
  onCloseListeners_->push_back(std::move(listener));
}

void StandardReactiveSocket::tryClientResume(
    const ResumeIdentificationToken& token,
    std::shared_ptr<FrameTransport> frameTransport,
    std::unique_ptr<ClientResumeStatusCallback> resumeCallback) {
  // TODO: verify/assert that the new frameTransport is on the same event base
  debugCheckCorrectExecutor();
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
  debugCheckCorrectExecutor();
  checkNotClosed();
  disconnect();
  // TODO: verify, we should not be receiving any frames, not a single one
  connection_->connect(std::move(frameTransport), /*sendPendingFrames=*/false);
  return connection_->resumeFromPositionOrClose(position);
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
  debugCheckCorrectExecutor();
  return connection_ ? connection_->duplexConnection() : nullptr;
}

void StandardReactiveSocket::debugCheckCorrectExecutor() const {
  DCHECK(
      !dynamic_cast<folly::EventBase*>(&executor_) ||
      dynamic_cast<folly::EventBase*>(&executor_)->isInEventBaseThread());
}

} // reactivesocket
