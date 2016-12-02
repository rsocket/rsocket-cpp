// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

#include "src/ConnectionSetupPayload.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/Stats.h"

namespace folly {
class Executor;
}

namespace reactivesocket {

class ConnectionAutomaton;
class DuplexConnection;
class RequestHandlerBase;
class ReactiveSocket;
class FrameSink;
enum class FrameType : uint16_t;
using StreamId = uint32_t;

folly::Executor& defaultExecutor();

// Client Side Keepalive Timer
class KeepaliveTimer {
 public:
  virtual ~KeepaliveTimer() = default;

  virtual std::chrono::milliseconds keepaliveTime() = 0;
  virtual void stop() = 0;
  virtual void start(const std::shared_ptr<FrameSink>& connection) = 0;
  virtual void keepaliveReceived() = 0;
};

using CloseListener = std::function<void(ReactiveSocket&)>;

// TODO(stupaq): consider using error codes in place of folly::exception_wrapper

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
          std::unique_ptr<KeepaliveTimer>(nullptr),
      const ResumeIdentificationToken& token = {
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

  static std::unique_ptr<ReactiveSocket> fromServerConnection(
      std::unique_ptr<DuplexConnection> connection,
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

  void close();

  void onClose(CloseListener listener);

  void metadataPush(std::unique_ptr<folly::IOBuf> metadata);

  void tryClientResume(
      std::unique_ptr<DuplexConnection> newConnection,
      const ResumeIdentificationToken& token);

 private:
  ReactiveSocket(
      bool isServer,
      std::unique_ptr<DuplexConnection> connection,
      std::shared_ptr<RequestHandlerBase> handler,
      Stats& stats,
      std::unique_ptr<KeepaliveTimer> keepaliveTimer);

  static bool createResponder(
      std::shared_ptr<RequestHandlerBase> handler,
      ConnectionAutomaton& connection,
      StreamId streamId,
      std::unique_ptr<folly::IOBuf> frame);
  std::shared_ptr<StreamState> resumeListener(
      const ResumeIdentificationToken& token);

  std::shared_ptr<RequestHandlerBase> handler_;
  const std::shared_ptr<KeepaliveTimer> keepaliveTimer_;
  const std::shared_ptr<ConnectionAutomaton> connection_;
  StreamId nextStreamId_;
};
}
