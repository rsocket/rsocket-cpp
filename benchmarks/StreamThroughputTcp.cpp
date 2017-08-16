// Copyright 2004-present Facebook. All Rights Reserved.

#include "benchmarks/StreamThroughput.h"

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

DEFINE_int32(items, 1000000, "number of items in stream");

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
    folly::EventBase* eventBase,
    folly::SocketAddress address) {
  auto factory =
      std::make_unique<TcpConnectionFactory>(*eventBase, std::move(address));
  return RSocket::createConnectedClient(std::move(factory)).get();
}

BENCHMARK(StreamThroughput, n) {
  folly::ScopedEventBaseThread worker;

  std::unique_ptr<RSocketServer> server;
  std::shared_ptr<RSocketClient> client;
  yarpl::Reference<BoundedSubscriber> subscriber;

  BENCHMARK_SUSPEND {
    LOG(INFO) << "  Running with " << FLAGS_items << " items";

    folly::SocketAddress address{"::1", static_cast<uint16_t>(FLAGS_port)};
    server = makeServer(std::move(address));

    folly::SocketAddress actual{"::1", *server->listeningPort()};
    client = makeClient(worker.getEventBase(), std::move(actual));

    subscriber = yarpl::make_ref<BoundedSubscriber>(FLAGS_items);
  }

  client->getRequester()
      ->requestStream(Payload("TcpStream"))
      ->subscribe(subscriber);

  subscriber->awaitTerminalEvent();
}
