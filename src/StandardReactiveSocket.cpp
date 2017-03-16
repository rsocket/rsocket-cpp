// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/StandardReactiveSocket.h"

#include <folly/Conv.h>
#include <folly/ExceptionWrapper.h>
#include <folly/Memory.h>
#include <folly/MoveWrapper.h>
#include <folly/io/async/EventBase.h>

#include "src/ClientResumeStatusCallback.h"
#include "src/ConnectionAutomaton.h"
#include "src/FrameTransport.h"
#include "src/RequestHandler.h"

namespace reactivesocket {

StandardReactiveSocket::~StandardReactiveSocket() {
  debugCheckCorrectExecutor();

  // Force connection closure, this will trigger terminal signals to be
  // delivered to all stream automata.
  close();
}

StandardReactiveSocket::StandardReactiveSocket(
    ReactiveSocketMode mode,
    std::shared_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    folly::Executor& executor)
    : handler_(handler),
      connection_(std::make_shared<ConnectionAutomaton>(
          executor,
          [this, handler](std::unique_ptr<folly::IOBuf> serializedFrame) {
            onConnectionFrame(handler, std::move(serializedFrame));
          },
          handler,
          std::bind(
              &StandardReactiveSocket::resumeListener,
              this,
              std::placeholders::_1),
          std::move(stats),
          std::move(keepaliveTimer),
          mode)),
      executor_(executor) {
  // TODO: In case of server, FrameSerializer should be ideally set after
  // inspecting the SETUP frame from the client
  connection_->setFrameSerializer(FrameSerializer::createCurrentVersion());
  debugCheckCorrectExecutor();
  connection_->stats().socketCreated();
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::fromClientConnection(
    folly::Executor& executor,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    ConnectionSetupPayload setupPayload,
    std::shared_ptr<Stats> stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer) {
  auto socket = disconnectedClient(
      executor,
      std::move(handler),
      std::move(stats),
      std::move(keepaliveTimer));
  socket->clientConnect(
      std::make_shared<FrameTransport>(std::move(connection)),
      std::move(setupPayload));
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::disconnectedClient(
    folly::Executor& executor,
    std::unique_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer) {
  std::unique_ptr<StandardReactiveSocket> socket(new StandardReactiveSocket(
      ReactiveSocketMode::CLIENT,
      std::move(handler),
      std::move(stats),
      std::move(keepaliveTimer),
      executor));
  socket->connection_->setFrameSerializer(
      FrameSerializer::createCurrentVersion());
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::fromServerConnection(
    folly::Executor& executor,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    bool isResumable) {
  // TODO: isResumable should come as a flag on Setup frame and it should be
  // exposed to the application code. We should then remove this parameter
  auto socket =
      disconnectedServer(executor, std::move(handler), std::move(stats));
  socket->serverConnect(
      std::make_shared<FrameTransport>(std::move(connection)), isResumable);
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::disconnectedServer(
    folly::Executor& executor,
    std::shared_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats) {
  std::unique_ptr<StandardReactiveSocket> socket(new StandardReactiveSocket(
      ReactiveSocketMode::SERVER,
      std::move(handler),
      std::move(stats),
      nullptr,
      executor));
  return socket;
}

std::shared_ptr<Subscriber<Payload>> StandardReactiveSocket::requestChannel(
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  return connection_->streamsFactory().createChannelRequester(
      std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestStream(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->streamsFactory().createStreamRequester(
      std::move(request), std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestResponse(
    Payload payload,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->streamsFactory().createRequestResponseRequester(
      std::move(payload), std::move(responseSink), executor_);
}

void StandardReactiveSocket::requestFireAndForget(Payload request) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  Frame_REQUEST_FNF frame(
      connection_->streamsFactory().getNextStreamId(),
      FrameFlags::EMPTY,
      std::move(std::move(request)));
  connection_->outputFrameOrEnqueue(
      connection_->frameSerializer().serializeOut(std::move(frame)));
}

void StandardReactiveSocket::metadataPush(
    std::unique_ptr<folly::IOBuf> metadata) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->outputFrameOrEnqueue(connection_->frameSerializer().serializeOut(
      Frame_METADATA_PUSH(std::move(metadata))));
}

void StandardReactiveSocket::onConnectionFrame(
    std::shared_ptr<RequestHandler> handler,
    std::unique_ptr<folly::IOBuf> serializedFrame) {
  debugCheckCorrectExecutor();
  auto type = connection_->frameSerializer().peekFrameType(*serializedFrame);
  switch (type) {
    case FrameType::SETUP: {
      Frame_SETUP frame;
      if (!connection_->deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      if (!!(frame.header_.flags_ & FrameFlags::LEASE)) {
        // TODO(yschimke) We don't have the correct lease and wait logic above
        // yet
        LOG(WARNING) << "ignoring setup frame with lease";
        //          connectionOutput_.onNext(
        //              Frame_ERROR::badSetupFrame("leases not supported")
        //                  .serializeOut());
        //          disconnect();
      }

      connection_->setFrameSerializer(FrameSerializer::createFrameSerializer(
          ProtocolVersion{frame.versionMajor_, frame.versionMinor_}));

      ConnectionSetupPayload setupPayload;
      frame.moveToSetupPayload(setupPayload);
      auto streamState =
          handler->handleSetupPayload(*this, std::move(setupPayload));

      // TODO(lehecka): use again
      // connection.useStreamState(streamState);
      break;
    }
    case FrameType::RESUME: {
      Frame_RESUME frame;
      if (!connection_->deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      auto resumed = handler->handleResume(
          *this,
          frame.token_,
          frame.lastReceivedServerPosition_,
          frame.clientPosition_);
      if (!resumed) {
        // TODO(lehecka): the "connection" and "this" arguments needs to be
        // cleaned up. It is not intuitive what is their lifetime.
        auto connectionCopy = std::move(connection_);
        connectionCopy->closeWithError(
            Frame_ERROR::connectionError("can not resume"));
      }
      break;
    }
    case FrameType::METADATA_PUSH: {
      Frame_METADATA_PUSH frame;
      if (!connection_->deserializeFrameOrError(
              frame, std::move(serializedFrame))) {
        return;
      }
      handler->handleMetadataPush(std::move(frame.metadata_));
      break;
    }

    case FrameType::REQUEST_CHANNEL:
    case FrameType::REQUEST_STREAM:
    case FrameType::REQUEST_RESPONSE:
    case FrameType::REQUEST_FNF:
    case FrameType::LEASE:
    case FrameType::KEEPALIVE:
    case FrameType::RESERVED:
    case FrameType::REQUEST_N:
    case FrameType::CANCEL:
    case FrameType::PAYLOAD:
    case FrameType::ERROR:
    case FrameType::RESUME_OK:
    case FrameType::EXT:
    default:
      auto connectionCopy = std::move(connection_);
      connectionCopy->closeWithError(Frame_ERROR::unexpectedFrame());
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
      setupPayload.resumable ? FrameFlags::RESUME_ENABLE : FrameFlags::EMPTY,
      FrameSerializer::kCurrentProtocolVersion.major,
      FrameSerializer::kCurrentProtocolVersion.minor,
      connection_->getKeepaliveTime(),
      std::numeric_limits<uint32_t>::max(),
      setupPayload.token,
      std::move(setupPayload.metadataMimeType),
      std::move(setupPayload.dataMimeType),
      std::move(setupPayload.payload));

  // TODO: when the server returns back that it doesn't support resumability, we
  // should retry without resumability

  // making sure we send setup frame first
  frameTransport->outputFrameOrEnqueue(
      connection_->frameSerializer().serializeOut(std::move(frame)));
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
  auto constexpr signal = StreamCompletionSignal::SOCKET_CLOSED;
  if (auto copy = std::move(connection_)) {
    copy->close(folly::exception_wrapper(), signal);
  }
}

void StandardReactiveSocket::disconnect() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->disconnect(folly::exception_wrapper());
}

void StandardReactiveSocket::closeConnectionError(const std::string& reason) {
  debugCheckCorrectExecutor();
  if (auto copy = std::move(connection_)) {
    auto err = Frame_ERROR::connectionError(reason);
    copy->closeWithError(std::move(err));
  }
}

std::shared_ptr<FrameTransport> StandardReactiveSocket::detachFrameTransport() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  return connection_->detachFrameTransport();
}

void StandardReactiveSocket::onConnected(std::function<void()> listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->addConnectedListener(std::move(listener));
}

void StandardReactiveSocket::onDisconnected(ErrorCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->addDisconnectedListener(std::move(listener));
}

void StandardReactiveSocket::onClosed(ErrorCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->addClosedListener(std::move(listener));
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
      connection_->frameSerializer().serializeOut(
          connection_->createResumeFrame(token)));

  // if the client was still connected we will disconnected the old connection
  // with a clear error message
  connection_->disconnect(
      std::runtime_error("resuming client on a different connection"));
  connection_->setResumable(true);
  connection_->reconnect(std::move(frameTransport), std::move(resumeCallback));
}

bool StandardReactiveSocket::tryResumeServer(
    std::shared_ptr<FrameTransport> frameTransport,
    ResumePosition serverPosition,
    ResumePosition clientPosition) {
  // TODO: verify/assert that the new frameTransport is on the same event base
  debugCheckCorrectExecutor();
  checkNotClosed();

  // if the server was still connected we will disconnected it with a clear
  // error message
  connection_->disconnect(
      std::runtime_error("resuming server on a different connection"));
  // TODO: verify, we should not be receiving any frames, not a single one
  connection_->connect(std::move(frameTransport), /*sendPendingFrames=*/false);
  return connection_->resumeFromPositionOrClose(serverPosition, clientPosition);
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

bool StandardReactiveSocket::isClosed() {
  debugCheckCorrectExecutor();
  return !static_cast<bool>(connection_);
}

} // reactivesocket
