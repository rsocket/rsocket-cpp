// Copyright 2004-present Facebook. All Rights Reserved.

#include <utility>

#include <folly/io/IOBuf.h>
#include <gmock/gmock.h>

#include "handlers/HelloStreamRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionAcceptor.h"

using namespace rsocket;
using namespace rsocket::tests;

TEST(RSocketClientServer, StartAndShutdown) {
  {
    TcpConnectionAcceptor::Options opts;
    opts.port = 9889;
    opts.threads = 2;

    // RSocket server accepting on TCP
    auto rs = RSocket::createServer(
        std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

    // global request handler
    auto handler = std::make_shared<HelloStreamRequestHandler>();
    // start server
    rs->start([handler](auto r) { return handler; });
  }
  // everything should cleanly shut down here
  // TODO anything to assert here?
}
