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

DEFINE_int32(port, 0, "port to connect to");

DEFINE_int32(
    items,
    1000000,
    "number of request-response requests to send, in total across all clients");

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
  folly::ScopedEventBaseThread worker;

  std::unique_ptr<RSocketServer> server;
  std::shared_ptr<RSocketClient> client;
  std::vector<yarpl::Reference<Observer>> observers;

  Latch latch{static_cast<size_t>(FLAGS_items)};

  BENCHMARK_SUSPEND {
    LOG(INFO) << "  Running with " << FLAGS_items << " items";

    folly::SocketAddress address{"::1", static_cast<uint16_t>(FLAGS_port)};
    server = makeServer(std::move(address));

    folly::SocketAddress actual{"::1", *server->listeningPort()};

    client = makeClient(worker.getEventBase(), std::move(actual));
  }

  for (int i = 0; i < FLAGS_items; ++i) {
    auto observer = yarpl::make_ref<Observer>(latch);
    observers.push_back(observer);

    client->getRequester()
        ->requestResponse(Payload("RequestResponseTcp"))
        ->subscribe(observer);
  }

  constexpr std::chrono::minutes timeout{5};
  if (!latch.timed_wait(timeout)) {
    LOG(ERROR) << "Timed out!";
  }
}
