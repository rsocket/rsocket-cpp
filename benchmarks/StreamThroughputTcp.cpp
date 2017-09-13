// Copyright 2004-present Facebook. All Rights Reserved.

#include "benchmarks/Fixture.h"
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

DEFINE_int32(server_threads, 8, "number of server threads to run");
DEFINE_int32(
    override_client_threads,
    0,
    "control the number of client threads (defaults to the number of clients)");
DEFINE_int32(clients, 10, "number of clients to run");
DEFINE_int32(items, 1000000, "number of items in stream, per client");
DEFINE_int32(streams, 1, "number of streams, per client");

BENCHMARK(StreamThroughput, n) {
  std::unique_ptr<Fixture> fixture;
  Fixture::Options opts;

  Latch latch{static_cast<size_t>(FLAGS_streams)};

  BENCHMARK_SUSPEND {
    auto responder =
        std::make_shared<FixedResponder>(std::string(kMessageLen, 'a'));

    opts.serverThreads = FLAGS_server_threads;
    opts.clients = FLAGS_clients;
    if (FLAGS_override_client_threads > 0) {
      opts.clientThreads = FLAGS_override_client_threads;
    }

    fixture = std::make_unique<Fixture>(opts, std::move(responder));

    LOG(INFO) << "Running:";
    LOG(INFO) << "  Server with " << opts.serverThreads << " threads.";
    LOG(INFO) << "  " << opts.clients << " clients across "
              << fixture->workers.size() << " threads.";
    LOG(INFO) << "  Running " << FLAGS_streams
              << " streams of " << FLAGS_items << " items each.";
  }

  for (size_t i = 0; i < FLAGS_streams; ++i) {
    for (auto& client : fixture->clients) {
      client->getRequester()
          ->requestStream(Payload("TcpStream"))
          ->subscribe(yarpl::make_ref<BoundedSubscriber>(latch, FLAGS_items));
    }
  }

  constexpr std::chrono::minutes timeout{5};
  if (!latch.timed_wait(timeout)) {
    LOG(ERROR) << "Timed out!";
  }
}
