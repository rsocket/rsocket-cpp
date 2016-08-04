// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/Stats.h"

namespace reactivesocket {

class ConnectionAutomaton;
class DuplexConnection;
class RequestHandler;
enum class FrameType : uint16_t;
using StreamId = uint32_t;

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
      std::unique_ptr<RequestHandler> handler,
      Stats& stats = Stats::noop());

  static std::unique_ptr<ReactiveSocket> fromServerConnection(
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      Stats& stats = Stats::noop());

  Subscriber<Payload>& requestChannel(Subscriber<Payload>& responseSink);

  void requestStream(Payload payload, Subscriber<Payload>& responseSink);

  void requestSubscription(Payload payload, Subscriber<Payload>& responseSink);

  void requestFireAndForget(Payload request);

 private:
  ReactiveSocket(
      bool isServer,
      std::unique_ptr<DuplexConnection> connection,
      std::unique_ptr<RequestHandler> handler,
      Stats& stats);

  bool createResponder(StreamId streamId, Payload& frame);

  const std::shared_ptr<ConnectionAutomaton> connection_;
  std::unique_ptr<RequestHandler> handler_;
  StreamId nextStreamId_;
  Stats& stats_;
};
}
