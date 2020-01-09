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

#include <iostream>
#include <sstream>
#include <thread>

#include <folly/init/Init.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "yarpl/Single.h"

using namespace rsocket;
using namespace yarpl::single;

DEFINE_int32(port, 9898, "port to connect to");

namespace {
class HelloRequestResponseResponder : public rsocket::RSocketResponder {
 public:
  std::shared_ptr<Single<Payload>> handleRequestResponse(
      Payload request,
      StreamId) override {
    std::cout << "HelloRequestResponseRequestResponder.handleRequestResponse "
              << request << std::endl;

    // string from payload data
    auto requestString = request.moveDataToString();

    return Single<Payload>::create(
        [name = std::move(requestString)](auto subscriber) {
          std::stringstream ss;
          ss << "Hello " << name << "!";
          std::string s = ss.str();
          subscriber->onSubscribe(SingleSubscriptions::empty());
          subscriber->onSuccess(Payload(s, "metadata"));
        });
  }
};
} // namespace

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  TcpConnectionAcceptor::Options opts;
  opts.address = folly::SocketAddress("::", FLAGS_port);
  opts.threads = 2;

  // RSocket server accepting on TCP
  auto rs = RSocket::createServer(
      std::make_unique<TcpConnectionAcceptor>(std::move(opts)));

  auto rawRs = rs.get();
  auto serverThread = std::thread([=] {
    // start accepting connections
    rawRs->startAndPark([](const rsocket::SetupParameters&) {
      return std::make_shared<HelloRequestResponseResponder>();
    });
  });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  rs->unpark();
  serverThread.join();

  return 0;
}
