// Copyright 2004-present Facebook. All Rights Reserved.

#include "benchmarks/Throughput.h"

#include <deque>

#include <folly/Baton.h>
#include <folly/Benchmark.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/Flowable.h"

using namespace rsocket;

constexpr size_t kMessageLen = 32;

DEFINE_int32(port, 0, "port to connect to");
DEFINE_int32(server_threads, 8, "number of server threads to run");
DEFINE_int32(
    override_client_threads,
    0,
    "control the number of client threads (defaults to the number of clients)");
DEFINE_int32(clients, 10, "number of clients to run");
DEFINE_int32(items, 1000000, "number of items in stream (per client)");

std::unique_ptr<RSocketServer> makeServer(folly::SocketAddress address) {
  TcpConnectionAcceptor::Options opts;
  opts.address = std::move(address);
  opts.threads = FLAGS_server_threads;

  auto acceptor = std::make_unique<TcpConnectionAcceptor>(std::move(opts));
  auto server = std::make_unique<RSocketServer>(std::move(acceptor));

  auto responder =
      std::make_shared<FixedResponder>(std::string(kMessageLen, 'a'));
  server->start([responder](const SetupParameters&) { return responder; });

  return server;
}

std::shared_ptr<RSocketClient> makeClient(
    folly::EventBase* eventBase,
    folly::SocketAddress address) {
  auto factory =
      std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address));
  return RSocket::createConnectedClient(std::move(factory)).get();
}

BENCHMARK(StreamThroughput, n) {
  std::unique_ptr<RSocketServer> server;

  std::deque<std::unique_ptr<folly::ScopedEventBaseThread>> workers;
  std::vector<std::shared_ptr<RSocketClient>> clients;
  std::vector<yarpl::Reference<BoundedSubscriber>> subscribers;

  BENCHMARK_SUSPEND {
    folly::SocketAddress address{"::1", static_cast<uint16_t>(FLAGS_port)};
    server = makeServer(std::move(address));

    folly::SocketAddress actual{"::1", *server->listeningPort()};

    auto const numWorkers = FLAGS_override_client_threads > 0
        ? FLAGS_override_client_threads
        : FLAGS_clients;

    for (int i = 0; i < numWorkers; ++i) {
      workers.push_back(std::make_unique<folly::ScopedEventBaseThread>(
          "rsocket-client-thread"));
    }

    for (int i = 0; i < FLAGS_clients; ++i) {
      auto worker = std::move(workers.front());
      workers.pop_front();
      clients.push_back(makeClient(worker->getEventBase(), actual));
      workers.push_back(std::move(worker));

      subscribers.push_back(yarpl::make_ref<BoundedSubscriber>(FLAGS_items));
    }

    LOG(INFO) << "Running:";
    LOG(INFO) << "  Server with " << FLAGS_server_threads << " threads.";
    LOG(INFO) << "  " << FLAGS_clients << " clients across " << numWorkers
              << " threads.";
    LOG(INFO) << "  Each receiving " << FLAGS_items << " items.";
  }

  for (int i = 0; i < FLAGS_clients; ++i) {
    clients[i]->getRequester()
      ->requestStream(Payload("TcpStream"))
      ->subscribe(subscribers[i]);
  }

  // TODO: Use a latch, don't serialize all waits.
  for (auto& subscriber : subscribers) {
    if (!subscriber->timedWait(std::chrono::minutes{5})) {
      LOG(ERROR) << "Timed out!";
    }
  }
}
