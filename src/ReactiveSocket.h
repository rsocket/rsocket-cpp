// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <list>
#include <memory>

#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/Stats.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class ClientResumeStatusCallback;
class ConnectionAutomaton;
class DuplexConnection;
class FrameTransport;
class RequestHandlerBase;
class ReactiveSocket;

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
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandlerBase> handler,
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload(),
      Stats& stats = Stats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr));

  static std::unique_ptr<ReactiveSocket> disconnectedClient(
      std::unique_ptr<RequestHandlerBase> handler,
      Stats& stats = Stats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr));

  static std::unique_ptr<ReactiveSocket> fromServerConnection(
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandlerBase> handler,
      Stats& stats = Stats::noop(),
      bool isResumable = false);

  static std::unique_ptr<ReactiveSocket> disconnectedServer(
      std::unique_ptr<RequestHandlerBase> handler,
      Stats& stats = Stats::noop());

  std::shared_ptr<Subscriber<Payload>> requestChannel(
      const std::shared_ptr<Subscriber<Payload>>& responseSink,
      folly::Executor& executor = defaultExecutor());

  void requestStream(
      Payload payload,
      const std::shared_ptr<Subscriber<Payload>>& responseSink,
      folly::Executor& executor = defaultExecutor());

  void requestSubscription(
      Payload payload,
      const std::shared_ptr<Subscriber<Payload>>& responseSink,
      folly::Executor& executor = defaultExecutor());

  void requestResponse(
      Payload payload,
      const std::shared_ptr<Subscriber<Payload>>& responseSink,
      folly::Executor& executor = defaultExecutor());

  void requestFireAndForget(Payload request);

  void clientConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload());

  void serverConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      bool isResumable);

  void close();
  void disconnect();
  std::shared_ptr<FrameTransport> detachFrameTransport();

  void onConnected(ReactiveSocketCallback listener);
  void onDisconnected(ReactiveSocketCallback listener);
  void onClosed(ReactiveSocketCallback listener);

  void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

  void tryClientResume(
      const ResumeIdentificationToken& token,
      std::unique_ptr<DuplexConnection> newConnection,
      std::unique_ptr<ClientResumeStatusCallback> resumeCallback);

  bool tryResumeServer(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumePosition position);

  const DuplexConnection& duplexConnection();

 private:
  ReactiveSocket(
      bool isServer,
      std::shared_ptr<RequestHandlerBase> handler,
      Stats& stats,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer);

  static void createResponder(
      std::shared_ptr<RequestHandlerBase> handler,
      ConnectionAutomaton& connection,
      StreamId streamId,
      std::unique_ptr<folly::IOBuf> frame);

  std::shared_ptr<StreamState> resumeListener(
      const ResumeIdentificationToken& token);

  std::function<void()> executeListenersFunc(
      std::shared_ptr<std::list<ReactiveSocketCallback>> listeners);

  void checkNotClosed() const;

  std::shared_ptr<RequestHandlerBase> handler_;

  std::shared_ptr<std::list<ReactiveSocketCallback>> onConnectListeners_{
      std::make_shared<std::list<ReactiveSocketCallback>>()};
  std::shared_ptr<std::list<ReactiveSocketCallback>> onDisconnectListeners_{
      std::make_shared<std::list<ReactiveSocketCallback>>()};
  std::shared_ptr<std::list<ReactiveSocketCallback>> onCloseListeners_{
      std::make_shared<std::list<ReactiveSocketCallback>>()};

  std::shared_ptr<ConnectionAutomaton> connection_;
  StreamId nextStreamId_;
};
}
