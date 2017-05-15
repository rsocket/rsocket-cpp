// Copyright 2004-present Facebook. All Rights Reserved.

#include "src/ReactiveSocket.h"

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

ReactiveSocket::~ReactiveSocket() {
  debugCheckCorrectExecutor();

  // Force connection closure, this will trigger terminal signals to be
  // delivered to all stream automata.
  close();
}

ReactiveSocket::ReactiveSocket(
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

std::unique_ptr<ReactiveSocket>
ReactiveSocket::fromClientConnection(
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

std::unique_ptr<ReactiveSocket>
ReactiveSocket::disconnectedClient(
    folly::Executor& executor,
    std::unique_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    std::unique_ptr<KeepaliveTimer> keepaliveTimer,
    ProtocolVersion protocolVersion) {
  std::unique_ptr<ReactiveSocket> socket(new ReactiveSocket(
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

std::unique_ptr<ReactiveSocket>
ReactiveSocket::fromServerConnection(
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

std::unique_ptr<ReactiveSocket>
ReactiveSocket::disconnectedServer(
    folly::Executor& executor,
    std::shared_ptr<RequestHandler> handler,
    std::shared_ptr<Stats> stats,
    ProtocolVersion protocolVersion) {
  std::unique_ptr<ReactiveSocket> socket(new ReactiveSocket(
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

std::shared_ptr<Subscriber<Payload>> ReactiveSocket::requestChannel(
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  return connection_->streamsFactory().createChannelRequester(
      std::move(responseSink), executor_);
}

void ReactiveSocket::requestStream(
    Payload request,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->streamsFactory().createStreamRequester(
      std::move(request), std::move(responseSink), executor_);
}

void ReactiveSocket::requestResponse(
    Payload payload,
    std::shared_ptr<Subscriber<Payload>> responseSink) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->streamsFactory().createRequestResponseRequester(
      std::move(payload), std::move(responseSink), executor_);
}

void ReactiveSocket::requestFireAndForget(Payload request) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->requestFireAndForget(std::move(request));
}

void ReactiveSocket::metadataPush(
    std::unique_ptr<folly::IOBuf> metadata) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->metadataPush(std::move(metadata));
}

void ReactiveSocket::clientConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload) {
  CHECK(frameTransport && !frameTransport->isClosed());
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->setResumable(setupPayload.resumable);

  if (setupPayload.protocolVersion != ProtocolVersion::Unknown) {
    CHECK_EQ(
        setupPayload.protocolVersion,
        connection_->getSerializerProtocolVersion());
  }

  connection_->setUpFrame(std::move(frameTransport), std::move(setupPayload));
}

void ReactiveSocket::serverConnect(
    std::shared_ptr<FrameTransport> frameTransport,
    const SocketParameters& socketParams) {
  debugCheckCorrectExecutor();
  connection_->setResumable(socketParams.resumable);
  connection_->connect(
      std::move(frameTransport), true, socketParams.protocolVersion);
}

void ReactiveSocket::close() {
  debugCheckCorrectExecutor();
  connection_->close(
      folly::exception_wrapper(), StreamCompletionSignal::SOCKET_CLOSED);
}

void ReactiveSocket::disconnect() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->disconnect(folly::exception_wrapper());
}

void ReactiveSocket::closeConnectionError(const std::string& reason) {
  debugCheckCorrectExecutor();
  connection_->closeWithError(Frame_ERROR::connectionError(reason));
}

std::shared_ptr<FrameTransport> ReactiveSocket::detachFrameTransport() {
  debugCheckCorrectExecutor();
  checkNotClosed();
  return connection_->detachFrameTransport();
}

void ReactiveSocket::onConnected(std::function<void()> listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->addConnectedListener(std::move(listener));
}

void ReactiveSocket::onDisconnected(ErrorCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->addDisconnectedListener(std::move(listener));
}

void ReactiveSocket::onClosed(ErrorCallback listener) {
  debugCheckCorrectExecutor();
  checkNotClosed();
  connection_->addClosedListener(std::move(listener));
}

void ReactiveSocket::tryClientResume(
    const ResumeIdentificationToken& token,
    std::shared_ptr<FrameTransport> frameTransport,
    std::unique_ptr<ClientResumeStatusCallback> resumeCallback) {
  // TODO: verify/assert that the new frameTransport is on the same event base
  debugCheckCorrectExecutor();
  checkNotClosed();
  CHECK(frameTransport && !frameTransport->isClosed());
  connection_->tryClientResume(
      token, std::move(frameTransport), std::move(resumeCallback));
}

bool ReactiveSocket::tryResumeServer(
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

void ReactiveSocket::checkNotClosed() const {
  CHECK(!connection_->isClosed()) << "ReactiveSocket already closed";
}

DuplexConnection* ReactiveSocket::duplexConnection() const {
  debugCheckCorrectExecutor();
  return connection_->duplexConnection();
}

void ReactiveSocket::debugCheckCorrectExecutor() const {
  DCHECK(
      !dynamic_cast<folly::EventBase*>(&executor_) ||
      dynamic_cast<folly::EventBase*>(&executor_)->isInEventBaseThread());
}

bool ReactiveSocket::isClosed() {
  debugCheckCorrectExecutor();
  return connection_->isClosed();
}

} // reactivesocket
