// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
#include <folly/Benchmark.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include <gflags/gflags.h>
#include <condition_variable>
#include <iostream>
#include <thread>

#include "rsocket/RSocket.h"
#include "rsocket/transports/tcp/TcpConnectionAcceptor.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/Flowable.h"
#include "yarpl/utils/ExceptionString.h"

using namespace rsocket;

#define MESSAGE_LENGTH (32)

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 0, "host:port to connect to");

DEFINE_int32(items, 1000000, "number of items in stream");

class BM_RequestHandler : public RSocketResponder {
 public:
  yarpl::Reference<yarpl::flowable::Flowable<Payload>> handleRequestStream(
      Payload,
      StreamId) override {
    return yarpl::flowable::Flowables::fromGenerator<Payload>(
        [] { return Payload(std::string(MESSAGE_LENGTH, 'a')); });
  }
};

class BM_Subscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  ~BM_Subscriber() {
    VLOG(2) << "BM_Subscriber destroy " << this;
  }

  explicit BM_Subscriber(size_t requested) : requested_{requested} {
    VLOG(2) << "BM_Subscriber " << this << " created with => "
            << "  Requested: " << requested_;
  }

  void onSubscribe(
      yarpl::Reference<yarpl::flowable::Subscription> subscription) override {
    VLOG(2) << "BM_Subscriber " << this << " onSubscribe";

    subscription_ = std::move(subscription);
    subscription_->request(requested_);
  }

  void onNext(Payload element) noexcept override {
    VLOG(2) << "BM_Subscriber " << this
            << " onNext as string: " << element.moveDataToString();

    if (received_.fetch_add(1) == requested_ - 1) {
      subscription_->cancel();
      baton_.post();
    }
  }

  void onComplete() noexcept override {
    VLOG(2) << "BM_Subscriber " << this << " onComplete";
    baton_.post();
  }

  void onError(std::exception_ptr ex) noexcept override {
    VLOG(2) << "BM_Subscriber " << this << " onError "
            << yarpl::exceptionStr(ex);
    baton_.post();
  }

  void awaitTerminalEvent() {
    VLOG(2) << "BM_Subscriber " << this << " block thread";
    baton_.wait();
    VLOG(2) << "BM_Subscriber " << this << " unblocked";
  }

 private:
  size_t requested_{0};
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  folly::Baton<std::atomic, false /* SinglePoster */> baton_;
  std::atomic<size_t> received_{0};
};

std::unique_ptr<RSocketServer> makeServer(folly::SocketAddress address) {
  TcpConnectionAcceptor::Options opts;
  opts.address = std::move(address);

  auto acceptor = std::make_unique<TcpConnectionAcceptor>(std::move(opts));

  auto server = std::make_unique<RSocketServer>(std::move(acceptor));

  auto responder = std::make_shared<BM_RequestHandler>();
  server->start([responder](const SetupParameters&) { return responder; });

  return server;
}

std::shared_ptr<RSocketClient> makeClient(folly::SocketAddress address) {
  auto factory = std::make_unique<TcpConnectionFactory>(std::move(address));
  return RSocket::createConnectedClient(std::move(factory)).get();
}

BENCHMARK(StreamThroughput, n) {
  std::unique_ptr<RSocketServer> server;
  std::shared_ptr<RSocketClient> client;
  yarpl::Reference<BM_Subscriber> subscriber;

  BENCHMARK_SUSPEND {
    LOG(INFO) << "  Running with " << FLAGS_items << " items";

    folly::SocketAddress address{FLAGS_host, static_cast<uint16_t>(FLAGS_port),
                                 true /* allowNameLookup */};
    server = makeServer(std::move(address));

    folly::SocketAddress actual{
        FLAGS_host, *server->listeningPort(), true /* allowNameLookup */};
    client = makeClient(std::move(actual));

    subscriber = yarpl::make_ref<BM_Subscriber>(FLAGS_items);
  }

  client->getRequester()
      ->requestStream(Payload("BM_Stream"))
      ->subscribe(subscriber);

  subscriber->awaitTerminalEvent();
}
