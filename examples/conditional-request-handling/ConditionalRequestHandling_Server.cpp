// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <iostream>

#include "JsonRequestHandler.h"
#include "TextRequestHandler.h"
#include "src/RSocket.h"
#include "src/RSocketErrors.h"
#include "src/transports/tcp/TcpConnectionAcceptor.h"

using namespace ::folly;
using namespace ::rsocket;

DEFINE_int32(port, 9898, "port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.port = FLAGS_port;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));
  // global request responders
  auto textResponder = std::make_shared<TextRequestResponder>();
  auto jsonResponder = std::make_shared<JsonRequestResponder>();
  // start accepting connections
  rs->startAndPark(
      [textResponder, jsonResponder](auto& setup) {
            if (setup.params().dataMimeType == "text/plain") {
              LOG(INFO) << "Connection Request => text/plain MimeType";
              setup.createRSocket(textResponder);
            } else if (setup.params().dataMimeType == "application/json") {
              LOG(INFO) << "Connection Request => application/json MimeType";
              setup.createRSocket(jsonResponder);
            } else {
              LOG(INFO) << "Connection Request => Unsupported MimeType"
                        << setup.params().dataMimeType;
              setup.error(UnsupportedSetupError("Unknown MimeType"));
            }
          });
}
