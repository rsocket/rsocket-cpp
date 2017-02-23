// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include "ServerRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionAcceptor.h"

using namespace ::reactivesocket;
using namespace ::folly;
using namespace ::rsocket;

DEFINE_int32(port, 9898, "port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;

  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(TcpConnectionAcceptor::create(FLAGS_port));
  // global request handler
  auto requestHandler = std::make_shared<ServerRequestHandler>();
  // start accepting connections
  rs->start([requestHandler](auto connectionRequest) {
    if (!connectionRequest->isResumptionAttempt()) {
      return requestHandler;
    } else {
      throw std::runtime_error("Unable to handle these SETUP params");
    }
  });

  std::string name;
  std::getline(std::cin, name);
}
