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
    : connection_(std::make_shared<ConnectionAutomaton>(
          executor,
          this,
          std::move(handler),
          std::move(stats),
          std::move(keepaliveTimer),
          mode)),
      executor_(executor) {
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
      std::move(keepaliveTimer),
      setupPayload.protocolVersion);
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
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    ProtocolVersion protocolVersion) {
  std::unique_ptr<StandardReactiveSocket> socket(new StandardReactiveSocket(
      ReactiveSocketMode::CLIENT,
      std::move(handler),
      std::move(stats),
      std::move(keepaliveTimer),
      executor));
  socket->connection_->setFrameSerializer(
      protocolVersion == ProtocolVersion::Unknown
          ? FrameSerializer::createCurrentVersion()
          : FrameSerializer::createFrameSerializer(protocolVersion));
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::fromServerConnection(
    folly::Executor& executor,
    std::unique_ptr<DuplexConnection> connection,
    std::unique_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    const SocketParameters& socketParameters) {
  // TODO: isResumable should come as a flag on Setup frame and it should be
  // exposed to the application code. We should then remove this parameter
  auto socket = disconnectedServer(
      executor,
      std::move(handler),
      std::move(stats),
      socketParameters.protocolVersion);

  socket->serverConnect(
      std::make_shared<FrameTransport>(std::move(connection)),
      socketParameters);
  return socket;
}

std::unique_ptr<StandardReactiveSocket>
StandardReactiveSocket::disconnectedServer(
    folly::Executor& executor,
    std::shared_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    ProtocolVersion protocolVersion) {
  std::unique_ptr<StandardReactiveSocket> socket(new StandardReactiveSocket(
      ReactiveSocketMode::SERVER,
      std::move(handler),
      std::move(stats),
      nullptr,
      executor));
  if (protocolVersion != ProtocolVersion::Unknown) {
    socket->connection_->setFrameSerializer(
        FrameSerializer::createFrameSerializer(protocolVersion));
  }
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

void StandardReactiveSocket::clientConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload) {
  CHECK(frameTransport && !frameTransport->isClosed());
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->setResumable(setupPayload.resumable);

  if (setupPayload.protocolVersion != ProtocolVersion::Unknown) {
    CHECK_EQ(
        setupPayload.protocolVersion,
        connection_->frameSerializer().protocolVersion());
  }

  auto protocolVersion = connection_->frameSerializer().protocolVersion();

  Frame_SETUP frame(
      setupPayload.resumable ? FrameFlags::RESUME_ENABLE : FrameFlags::EMPTY,
      protocolVersion.major,
      protocolVersion.minor,
      connection_->getKeepaliveTime(),
      Frame_SETUP::kMaxLifetime,
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
  connection_->connect(
      std::move(frameTransport), true, ProtocolVersion::Unknown);
}

void StandardReactiveSocket::serverConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    const SocketParameters& socketParams) {
  debugCheckCorrectExecutor();
  connection_->setResumable(socketParams.resumable);
  connection_->connect(
      std::move(frameTransport), true, socketParams.protocolVersion);
}

void StandardReactiveSocket::close() {
  debugCheckCorrectExecutor();
  connection_->close(
      folly::exception_wrapper(), StreamCompletionSignal::SOCKET_CLOSED);
}

void StandardReactiveSocket::disconnect() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->disconnect(folly::exception_wrapper());
}

void StandardReactiveSocket::closeConnectionError(const std::string& reason) {
  debugCheckCorrectExecutor();
  connection_->closeWithError(Frame_ERROR::connectionError(reason));
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
    const ResumeParameters& resumeParams) {
  CHECK(resumeParams.protocolVersion != ProtocolVersion::Unknown);

  // TODO: verify/assert that the new frameTransport is on the same event base
  debugCheckCorrectExecutor();
  checkNotClosed();

  // if the server was still connected we will disconnected it with a clear
  // error message
  connection_->disconnect(
      std::runtime_error("resuming server on a different connection"));
  // TODO: verify, we should not be receiving any frames, not a single one
  return connection_->connect(
             std::move(frameTransport),
             /*sendPendingFrames=*/false,
             resumeParams.protocolVersion) &&
      connection_->resumeFromPositionOrClose(
          resumeParams.serverPosition, resumeParams.clientPosition);
}

void StandardReactiveSocket::checkNotClosed() const {
  CHECK(!connection_->isClosed()) << "ReactiveSocket already closed";
}

DuplexConnection* StandardReactiveSocket::duplexConnection() const {
  debugCheckCorrectExecutor();
  return connection_->duplexConnection();
}

void StandardReactiveSocket::debugCheckCorrectExecutor() const {
  DCHECK(
      !dynamic_cast<folly::EventBase*>(&executor_) ||
      dynamic_cast<folly::EventBase*>(&executor_)->isInEventBaseThread());
}

bool StandardReactiveSocket::isClosed() {
  debugCheckCorrectExecutor();
  return connection_->isClosed();
}

} // reactivesocket
