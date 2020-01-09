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

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/examples/util/ExampleSubscriber.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

#include "yarpl/Flowable.h"

using namespace rsocket;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  folly::ScopedEventBaseThread worker;

  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);

  auto client = RSocket::createConnectedClient(
                    std::make_unique<TcpConnectionFactory>(
                        *worker.getEventBase(), std::move(address)))
                    .get();

  client->getRequester()
      ->requestStream(Payload("Jane"))
      ->subscribe([](Payload p) {
        std::cout << "Received: " << p.moveDataToString() << std::endl;
      });

  // Wait for a newline on the console to terminate the server.
  std::getchar();

  return 0;
}
