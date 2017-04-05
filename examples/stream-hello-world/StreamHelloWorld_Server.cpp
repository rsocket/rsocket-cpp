// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>

#include <folly/init/Init.h>

#include "HelloStreamRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionAcceptor.h"

using namespace reactivesocket;
using namespace rsocket;

DEFINE_int32(port, 9898, "port to connect to");

int main(int argc, char* argv[]) {
  folly::init(&argc, &argv);

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(TcpConnectionAcceptor::create(FLAGS_port));
  // global request handler
  auto handler = std::make_shared<HelloStreamRequestHandler>();
  // start accepting connections
  rs->startAndPark([handler](auto r) { return handler; });
}
