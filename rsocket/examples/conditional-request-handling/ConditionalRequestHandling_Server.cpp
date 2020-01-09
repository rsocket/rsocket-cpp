// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>
#include <iostream>

#include "JsonRequestHandler.h"
#include "TextRequestHandler.h"
#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"

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
