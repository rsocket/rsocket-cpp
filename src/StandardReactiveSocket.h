// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <list>
#include <memory>

#include "src/Common.h"
#include "src/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "src/ReactiveSocket.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/Stats.h"
#include "src/StreamsFactory.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class ClientResumeStatusCallback;
class ConnectionAutomaton;
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
class StandardReactiveSocket : public ReactiveSocket {
 public:
  StandardReactiveSocket(StandardReactiveSocket&&) = delete;
  StandardReactiveSocket& operator=(StandardReactiveSocket&&) = delete;
  StandardReactiveSocket(const StandardReactiveSocket&) = delete;
  StandardReactiveSocket& operator=(const StandardReactiveSocket&) = delete;

  ~StandardReactiveSocket();

  static std::unique_ptr<StandardReactiveSocket> fromClientConnection(
      folly::Executor& executor,
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload(),
      Stats& stats = Stats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr));

  static std::unique_ptr<StandardReactiveSocket> disconnectedClient(
      folly::Executor& executor,
      std::unique_ptr<RequestHandler> handler,
      Stats& stats = Stats::noop(),
      std::unique_ptr<KeepaliveTimer> keepaliveTimer =
          std::unique_ptr<KeepaliveTimer>(nullptr));

  static std::unique_ptr<StandardReactiveSocket> fromServerConnection(
      folly::Executor& executor,
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      Stats& stats = Stats::noop(),
      bool isResumable = false);

  static std::unique_ptr<StandardReactiveSocket> disconnectedServer(
      folly::Executor& executor,
      std::unique_ptr<RequestHandler> handler,
      Stats& stats = Stats::noop());

  std::shared_ptr<Subscriber<Payload>> requestChannel(
      std::shared_ptr<Subscriber<Payload>> responseSink) override;

  void requestStream(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink) override;

  void requestSubscription(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink) override;

  void requestResponse(
      Payload payload,
      std::shared_ptr<Subscriber<Payload>> responseSink) override;

  void requestFireAndForget(Payload request) override;

  void metadataPush(std::unique_ptr<folly::IOBuf> metadata) override;

  void clientConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      ConnectionSetupPayload setupPayload = ConnectionSetupPayload()) override;

  void serverConnect(
      std::shared_ptr<FrameTransport> frameTransport,
      bool isResumable) override;

  void close() override;
  void disconnect() override;
  std::shared_ptr<FrameTransport> detachFrameTransport() override;

  void onConnected(ReactiveSocketCallback listener) override;
  void onDisconnected(ReactiveSocketCallback listener) override;
  void onClosed(ReactiveSocketCallback listener) override;

  void tryClientResume(
      const ResumeIdentificationToken& token,
      std::shared_ptr<FrameTransport> frameTransport,
      std::unique_ptr<ClientResumeStatusCallback> resumeCallback) override;

  bool tryResumeServer(
      std::shared_ptr<FrameTransport> frameTransport,
      ResumePosition position) override;

  folly::Executor& executor() override {
    return executor_;
  }

  DuplexConnection* duplexConnection() const override;

 private:
  StandardReactiveSocket(
      ReactiveSocketMode mode,
      std::shared_ptr<RequestHandler> handler,
      Stats& stats,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer,
      folly::Executor& executor);

  void createResponder(
      std::shared_ptr<RequestHandler> handler,
      ConnectionAutomaton& connection,
      StreamId streamId,
      std::unique_ptr<folly::IOBuf> frame);

  std::shared_ptr<StreamState> resumeListener(
      const ResumeIdentificationToken& token);

  std::function<void()> executeListenersFunc(
      std::shared_ptr<std::list<ReactiveSocketCallback>> listeners);

  void checkNotClosed() const;
  void debugCheckCorrectExecutor() const;

  std::shared_ptr<RequestHandler> handler_;

  std::shared_ptr<std::list<ReactiveSocketCallback>> onConnectListeners_{
      std::make_shared<std::list<ReactiveSocketCallback>>()};
  std::shared_ptr<std::list<ReactiveSocketCallback>> onDisconnectListeners_{
      std::make_shared<std::list<ReactiveSocketCallback>>()};
  std::shared_ptr<std::list<ReactiveSocketCallback>> onCloseListeners_{
      std::make_shared<std::list<ReactiveSocketCallback>>()};

  std::shared_ptr<ConnectionAutomaton> connection_;
  StreamsFactory streamsFactory_;
  folly::Executor& executor_;
};
}
