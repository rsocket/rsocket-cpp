// Copyright 2004-present Facebook. All Rights Reserved.

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
    subscription_ = std::move(subscription);
  }

  void onSuccess(Payload) override {
    latch_.post();
  }

  void onError(folly::exception_wrapper) override {
    latch_.post();
  }

 private:
  Latch& latch_;

  yarpl::Reference<yarpl::single::SingleSubscription> subscription_;
};

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
    folly::EventBase* evb,
    folly::SocketAddress address) {
  auto factory =
      std::make_unique<TcpConnectionFactory>(*evb, std::move(address));
  return RSocket::createConnectedClient(std::move(factory)).get();
}

BENCHMARK(RequestResponseThroughput, n) {
  std::unique_ptr<RSocketServer> server;

  std::deque<std::unique_ptr<folly::ScopedEventBaseThread>> workers;
  std::vector<std::shared_ptr<RSocketClient>> clients;
  std::vector<yarpl::Reference<Observer>> observers;

  Latch latch{static_cast<size_t>(FLAGS_items)};

  BENCHMARK_SUSPEND {
    folly::SocketAddress address{"::1", 0};
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
    }

    LOG(INFO) << "Running:";
    LOG(INFO) << "  Server with " << FLAGS_server_threads << " threads.";
    LOG(INFO) << "  " << FLAGS_clients << " clients across " << numWorkers
              << " threads.";
    LOG(INFO) << "  Running " << FLAGS_items << " requests in total";
  }

  for (int i = 0; i < FLAGS_items; ++i) {
    auto& client = clients[i % clients.size()];
    client->getRequester()
        ->requestResponse(Payload("RequestResponseTcp"))
        ->subscribe(yarpl::make_ref<Observer>(latch));
  }

  constexpr std::chrono::minutes timeout{5};
  if (!latch.timed_wait(timeout)) {
    LOG(ERROR) << "Timed out!";
  }
}
