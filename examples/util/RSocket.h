// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/ExceptionWrapper.h>
#include <folly/futures/Future.h>
#include "examples/util/TcpConnectionFactory.h"
#include "src/Payload.h"
#include "src/ReactiveStreamsCompat.h"
#include "src/StandardReactiveSocket.h"

using namespace ::reactivesocket;

/**
 * Simplified API for client/server
 */
namespace rsocket {

class RSocket {
 public:
  static std::unique_ptr<RSocket> createClientFactory(
      std::unique_ptr<ConnectionFactory> connection);
  RSocket(std::unique_ptr<ConnectionFactory> connection);
  ~RSocket();
  /*
   * Connect asynchronously and return a Future
   * which will deliver the RSocket
   */
  Future<std::shared_ptr<StandardReactiveSocket>> connect(
      ScopedEventBaseThread& eventBaseThread);

 private:
  std::unique_ptr<ConnectionFactory> lazyConnection;
  std::shared_ptr<StandardReactiveSocket> rs;
};
}