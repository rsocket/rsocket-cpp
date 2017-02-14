// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>
#include "JsonRequestHandler.h"
#include "TextRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/RSocketErrors.h"
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
  // global request handlers
  auto textHandler = std::make_shared<TextRequestHandler>();
  auto jsonHandler = std::make_shared<JsonRequestHandler>();
  // start accepting connections
  rs->startAndPark(
      [textHandler, jsonHandler](std::unique_ptr<ConnectionSetupRequest> r)
          -> std::shared_ptr<RequestHandler> {
            if (r->getDataMimeType() == "text/plain") {
              LOG(INFO) << "Connection Request => text/plain MimeType";
              return textHandler;
            } else if (r->getDataMimeType() == "application/json") {
              LOG(INFO) << "Connection Request => application/json MimeType";
              return jsonHandler;
            } else {
              LOG(INFO) << "Connection Request => Unsupported MimeType"
                        << r->getDataMimeType();
              throw UnsupportedSetupError("Unknown MimeType");
            }
          });
}
