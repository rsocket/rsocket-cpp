// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <iostream>

#include "JsonRequestHandler.h"
#include "TextRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"

using namespace ::folly;
using namespace ::rsocket;

DEFINE_int32(port, 9898, "port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress("::", FLAGS_port);

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  rs->startAndPark(
      [](const rsocket::SetupParameters& params)
          -> std::shared_ptr<RSocketResponder> {
        LOG(INFO) << "Connection Request; MimeType : " << params.dataMimeType;
        if (params.dataMimeType == "text/plain") {
          return std::make_shared<TextRequestResponder>();
        } else if (params.dataMimeType == "application/json") {
          return std::make_shared<JsonRequestResponder>();
        } else {
          throw RSocketException("Unknown MimeType");
        }
      });
}
