// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "rsocket/ConnectionAcceptor.h"
#include "rsocket/ConnectionRequest.h"
#include "src/RequestHandler.h"
#include "src/ServerConnectionAcceptor.h"
#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

namespace rsocket {

using OnSetupNewSocket = std::function<void(
    std::shared_ptr<FrameTransport> frameTransport,
    ConnectionSetupPayload setupPayload,
    folly::EventBase&)>;

/**
 * API for starting an RSocket server. Returned from RSocket::createServer.
 */
class RSocketServer {
  // TODO resumability
  // TODO concurrency (number of threads)

 public:
  RSocketServer(std::unique_ptr<ConnectionAcceptor>);

  void start(std::function<std::shared_ptr<RequestHandler>(
                 std::unique_ptr<ConnectionRequest>)>);
  void shutdown();

 private:
  std::unique_ptr<ConnectionAcceptor> lazyAcceptor;
  std::unique_ptr<reactivesocket::ServerConnectionAcceptor> acceptor_;
  std::vector<std::unique_ptr<ReactiveSocket>> reactiveSockets_;
  bool shuttingDown{false};

  void removeSocket(ReactiveSocket& socket);
  void addSocket(std::unique_ptr<ReactiveSocket> socket);
};
}