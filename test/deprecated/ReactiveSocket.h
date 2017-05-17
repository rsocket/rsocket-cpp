// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include "src/Payload.h"
#include "src/RSocketParameters.h"
#include "src/RSocketStats.h"
#include "src/internal/Common.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace folly {
class Executor;
}

namespace rsocket {

class ClientResumeStatusCallback;
class RSocketStateMachine;
class DuplexConnection;
class FrameTransport;
class RequestHandler;

folly::Executor& defaultExecutor();

// TODO eliminate this class and use RSocketStateMachine directly
class ReactiveSocket {
 public:
  ReactiveSocket(ReactiveSocket&&) = delete;
  ReactiveSocket& operator=(ReactiveSocket&&) = delete;
  ReactiveSocket(const ReactiveSocket&) = delete;
  ReactiveSocket& operator=(const ReactiveSocket&) = delete;

  ~ReactiveSocket();

  static std::unique_ptr<ReactiveSocket> fromClientConnection(
      folly::Executor& executor,
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      SetupParameters setupPayload = SetupParameters(),
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr));

  static std::unique_ptr<ReactiveSocket> disconnectedClient(
      folly::Executor& executor,
      std::unique_ptr<RequestHandler> handler,
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr),
      ProtocolVersion protocolVersion = ProtocolVersion::Unknown);

  static std::unique_ptr<ReactiveSocket> fromServerConnection(
      folly::Executor& executor,
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop(),
      const RSocketParameters& socketParameters =
          RSocketParameters(/*resumable=*/false, ProtocolVersion::Unknown));

  static std::unique_ptr<ReactiveSocket> disconnectedServer(
      folly::Executor& executor,
      std::shared_ptr<RequestHandler> handler,
      std::shared_ptr<RSocketStats> stats = RSocketStats::noop(),
      ProtocolVersion protocolVersion = ProtocolVersion::Unknown);

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> requestChannel(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void requestStream(
      Payload payload,
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void requestResponse(
      Payload payload,
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> responseSink);

  void requestFireAndForget(Payload request);

  void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

  void clientConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      SetupParameters setupPayload = SetupParameters());

  void serverConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      const RSocketParameters& socketParams);

  void close();
  void disconnect();

  void closeConnectionError(const std::string& reason);

  std::shared_ptr<FrameTransport> detachFrameTransport();

  void onConnected(std::function<void()> listener);
  void onDisconnected(ErrorCallback listener);
  void onClosed(ErrorCallback listener);

  void tryClientResume(
      const ResumeIdentificationToken& token,
      std::shared_ptr<FrameTransport> frameTransport,
      std::unique_ptr<ClientResumeStatusCallback> resumeCallback);

  bool tryResumeServer(
      std::shared_ptr<FrameTransport> frameTransport,
      const ResumeParameters& resumeParams);

  folly::Executor& executor() {
    return executor_;
  }

  DuplexConnection* duplexConnection() const;
  bool isClosed();

 private:
  ReactiveSocket(
      ReactiveSocketMode mode,
      std::shared_ptr<RequestHandler> handler,
      std::shared_ptr<RSocketStats> stats,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer,
      folly::Executor& executor);

  void checkNotClosed() const;
  void debugCheckCorrectExecutor() const;

  std::shared_ptr<RSocketStateMachine> connection_;
  folly::Executor& executor_;
};
}
