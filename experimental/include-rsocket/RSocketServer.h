// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "ServerConnectionAcceptor.h"
#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

namespace rsocket {

// TODO why does we need to create a new RequestHandler for each RSocket?
// TODO can we instead make there be a "ConnectionSetupHandler" which
// TODO returns a RequestHandler that can be shared across many?
using HandlerFactory = std::function<std::unique_ptr<RequestHandler>()>;

class RSocketServer {
 public:
  RSocketServer(std::unique_ptr<ServerConnectionAcceptor>, HandlerFactory);
  ~RSocketServer() = default;

  void start();
  void shutdown();

 private:
  std::unique_ptr<ServerConnectionAcceptor> lazyAcceptor;
  HandlerFactory handlerFactory;
  std::vector<std::unique_ptr<StandardReactiveSocket>> reactiveSockets_;
  bool shuttingDown{false};

  void removeSocket(ReactiveSocket& socket);
};
}