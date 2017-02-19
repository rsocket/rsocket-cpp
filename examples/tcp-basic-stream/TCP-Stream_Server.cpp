// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/io/async/AsyncServerSocket.h>
#include <iostream>
#include "ServerRequestHandler.h"
#include "examples/util/RSocket.h"
#include "examples/util/TcpServerConnectionAcceptor.h"
#include "src/framed/FramedDuplexConnection.h"
#include "src/tcp/TcpDuplexConnection.h"

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

  auto rs = RSocket::createServer(
      ServerConnectionAcceptor::tcpServer(FLAGS_port),
      []() { return std::make_unique<ServerRequestHandler>(); });
  rs->start();

  // TODO should we instead have a ConnectionSetupHandler that decides what
  // RequestHandler to return?
  // example from java
  //    StartedServer s = ReactiveSocketServer
  //            .create(TcpTransportServer.create(port))
  //            .start((ConnectionSetupPayload setup, ReactiveSocket
  //            reactiveSocket) -> {

  std::string name;
  std::getline(std::cin, name);
}
