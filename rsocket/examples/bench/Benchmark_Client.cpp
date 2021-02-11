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

#include "yarpl/Single.h"

using namespace rsocket;
using namespace std::chrono;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

DEFINE_int32(payload_size, 4096, "payload size in bytes");
DEFINE_int32(threads, 10, "number of client threads to run");
DEFINE_int32(clients, 1, "number of clients to run");

DEFINE_int32(items, 1000000, "number of items to fire-and-forget, per client");


static std::shared_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    folly::SocketAddress address) {
  auto factory =
      std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address));
  return RSocket::createConnectedClient(std::move(factory)).get();
}

int main(int argc, char* argv[]) {
    FLAGS_logtostderr = true;
    FLAGS_minloglevel = 0;
    folly::init(&argc, &argv);

    std::vector<std::shared_ptr<RSocketClient>> clients;
    std::deque<std::unique_ptr<folly::ScopedEventBaseThread>> workers;

    folly::SocketAddress address;
    address.setFromHostPort(FLAGS_host, FLAGS_port);

    auto const numWorkers = FLAGS_threads;
    for (size_t i = 0; i < numWorkers; ++i) {
        workers.push_back(std::make_unique<folly::ScopedEventBaseThread>("rsocket-client-thread"));
    }

    for (size_t i = 0; i < FLAGS_clients; ++i) {
        auto worker = std::move(workers.front());
        workers.pop_front();
        clients.push_back(makeClient(worker->getEventBase(), address));
        workers.push_back(std::move(worker));
    }

    auto start = high_resolution_clock::now();

    auto ioBuf = new char[FLAGS_payload_size];

    for (int i = 0; i < FLAGS_items; ++i) {
        for (auto& client : clients) {
            client->getRequester()
                ->fireAndForget(Payload(ioBuf))
                ->subscribe(std::make_shared<yarpl::single::SingleObserverBase<void>>());
        }
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    delete ioBuf;

    printf("Sent %d items in %lu ms\n", FLAGS_items * FLAGS_clients, duration.count());

    auto total_bytes = (long) (FLAGS_items * FLAGS_clients * FLAGS_payload_size);
    printf("KB/s: %lu\n", (total_bytes * 1000) / (duration.count()  * 1024));

    return 0;
}
