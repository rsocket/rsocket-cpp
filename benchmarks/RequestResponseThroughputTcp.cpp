// Copyright 2004-present Facebook. All Rights Reserved.

#include "benchmarks/Fixture.h"
#include "benchmarks/Latch.h"
#include "benchmarks/Throughput.h"

#include <folly/Benchmark.h>
#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/Single.h"

using namespace rsocket;

constexpr size_t kMessageLen = 32;

DEFINE_int32(server_threads, 8, "number of server threads to run");
DEFINE_int32(
    override_client_threads,
    0,
    "control the number of client threads (defaults to the number of clients)");
DEFINE_int32(clients, 10, "number of clients to run");
DEFINE_int32(
    items,
    1000000,
    "number of request-response requests to send, in total");

class Observer : public yarpl::single::SingleObserver<Payload> {
 public:
  explicit Observer(Latch& latch) : latch_{latch} {}

  void onSubscribe(yarpl::Reference<yarpl::single::SingleSubscription>
                       subscription) override {
    yarpl::single::SingleObserver<Payload>::onSubscribe(std::move(subscription));
  }

  void onSuccess(Payload) override {
    latch_.post();
    yarpl::single::SingleObserver<Payload>::onSuccess({});
  }

  void onError(folly::exception_wrapper) override {
    latch_.post();
    yarpl::single::SingleObserver<Payload>::onError({});
  }

 private:
  Latch& latch_;
};

BENCHMARK(RequestResponseThroughput, n) {
  std::unique_ptr<Fixture> fixture;
  Fixture::Options opts;

  Latch latch{static_cast<size_t>(FLAGS_items)};

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
    LOG(INFO) << "  Running " << FLAGS_items << " requests in total";
  }

  for (int i = 0; i < FLAGS_items; ++i) {
    auto& client = fixture->clients[i % opts.clients];
    client->getRequester()
        ->requestResponse(Payload("RequestResponseTcp"))
        ->subscribe(yarpl::make_ref<Observer>(latch));
  }

  constexpr std::chrono::minutes timeout{5};
  if (!latch.timed_wait(timeout)) {
    LOG(ERROR) << "Timed out!";
  }
}
