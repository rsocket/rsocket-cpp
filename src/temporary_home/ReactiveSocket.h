// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include "src/internal/Common.h"
#include "src/temporary_home/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "Stats.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscription.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class ClientResumeStatusCallback;
class RSocketStateMachine;
class DuplexConnection;
class FrameTransport;
class RequestHandler;

folly::Executor& defaultExecutor();

// TODO(stupaq): Here is some heavy problem with the recursion on shutdown.
// Giving someone ownership over this object would probably lead to a deadlock
// (or a crash) if one calls ::~ReactiveSocket from a terminal signal handler
// of any of the connections. It seems natural to follow ReactiveStreams
// specification and make this object "self owned", so that we don't need to
// perform all of the cleanup from a d'tor. We could give out a "proxy" object
// that (internally) holds a weak reference (conceptually, not std::weak_ptr)
// and forbids any interactions with the socket after the shutdown procedure has
// been initiated.
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
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload(),
      std::shared_ptr<Stats> stats = Stats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr));

  static std::unique_ptr<ReactiveSocket> disconnectedClient(
      folly::Executor& executor,
      std::unique_ptr<RequestHandler> handler,
      std::shared_ptr<Stats> stats = Stats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr),
      ProtocolVersion protocolVersion = ProtocolVersion::Unknown);

  static std::unique_ptr<ReactiveSocket> fromServerConnection(
      folly::Executor& executor,
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      std::shared_ptr<Stats> stats = Stats::noop(),
      const SocketParameters& socketParameters =
          SocketParameters(/*resumable=*/false, ProtocolVersion::Unknown));

  static std::unique_ptr<ReactiveSocket> disconnectedServer(
      folly::Executor& executor,
      std::shared_ptr<RequestHandler> handler,
      std::shared_ptr<Stats> stats = Stats::noop(),
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
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload());

  void serverConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      const SocketParameters& socketParams);

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
      std::shared_ptr<Stats> stats,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer,
      folly::Executor& executor);

  void checkNotClosed() const;
  void debugCheckCorrectExecutor() const;

  std::shared_ptr<RSocketStateMachine> connection_;
  folly::Executor& executor_;
};
}
