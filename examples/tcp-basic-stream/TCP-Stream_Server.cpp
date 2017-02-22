// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include "ServerRequestHandler.h"
#include "experimental/include-rsocket/RSocket.h"
#include "experimental/rsocket-src/TcpServerConnectionAcceptor.h"

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

  //    auto a = ServerConnectionAcceptor::tcpServer(FLAGS_port);
  //    a->start([](auto c, auto eb) {
  //        return new RSocket(std::move(c), std::move(eb));
  //    });

  auto rs = RSocket::createServer(
      TcpServerConnectionAcceptor::create(FLAGS_port),
      []() { return std::make_unique<ServerRequestHandler>(); });
  rs->start();

  //    auto rs = RSocket::startServer(
  //            ServerConnectionAcceptor::tcpServer(FLAGS_port),
  //            [](SetupPayload setup, RSocket socket) {
  //                if(setup ... ) {
  //                    socket->requestResponse(...);
  //                    return std::make_unique<ServerRequestHandler>();
  //                } else {
  //                    return std::make_unique<ServerRequestHandler>();
  //                }
  //            });
  //

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
