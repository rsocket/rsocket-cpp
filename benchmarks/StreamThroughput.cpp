// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Benchmark.h>
#include <folly/init/Init.h>
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

class BM_Subscription : public yarpl::flowable::Subscription {
 public:
  explicit BM_Subscription(
      yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber,
      size_t length)
      : subscriber_(std::move(subscriber)),
        data_(length, 'a'),
        cancelled_(false) {}

 private:
  void request(int64_t n) noexcept override {
    VLOG(3) << "requested=" << n << " currentElem=" << currentElem_;

    for (int64_t i = 0; i < n; i++) {
      if (cancelled_) {
        VLOG(3) << "emission stopped by cancellation";
        return;
      }
      subscriber_->onNext(Payload(data_));
      currentElem_++;
    }
  }

  void cancel() noexcept override {
    VLOG(3) << "cancellation received";
    cancelled_ = true;
  }

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber_;
  std::string data_;
  size_t currentElem_ = 0;
  std::atomic_bool cancelled_;
};

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

  explicit BM_Subscriber(int initialRequest)
      : initialRequest_(initialRequest),
        thresholdForRequest_(initialRequest * 0.75),
        received_(0) {
    VLOG(2) << "BM_Subscriber " << this << " created with => "
            << "  Initial Request: " << initialRequest
            << "  Threshold for re-request: " << thresholdForRequest_;
  }

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override {
    VLOG(2) << "BM_Subscriber " << this << " onSubscribe";
    subscription_ = std::move(subscription);
    requested_ = initialRequest_;
    subscription_->request(initialRequest_);
  }

  void onNext(Payload element) noexcept override {
    VLOG(2) << "BM_Subscriber " << this
            << " onNext as string: " << element.moveDataToString();

    received_.store(received_ + 1, std::memory_order_release);

    if (--requested_ == thresholdForRequest_) {
      int toRequest = (initialRequest_ - thresholdForRequest_);
      VLOG(2) << "BM_Subscriber " << this << " requesting " << toRequest
              << " more items";
      requested_ += toRequest;
      subscription_->request(toRequest);
    };

    if (cancel_) {
      subscription_->cancel();

      terminated_ = true;
      terminalEventCV_.notify_all();
    }
  }

  void onComplete() noexcept override {
    VLOG(2) << "BM_Subscriber " << this << " onComplete";
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void onError(std::exception_ptr ex) noexcept override {
    VLOG(2) << "BM_Subscriber " << this << " onError "
            << yarpl::exceptionStr(ex);
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void awaitTerminalEvent() {
    VLOG(2) << "BM_Subscriber " << this << " block thread";
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);
    // if shutdown gets implemented this would then be released by it
    terminalEventCV_.wait(lk, [this] { return terminated_; });
    VLOG(2) << "BM_Subscriber " << this << " unblocked";
  }

  void cancel() {
    cancel_ = true;
  }

  size_t received() {
    return received_.load(std::memory_order_acquire);
  }

 private:
  int initialRequest_;
  int thresholdForRequest_;
  int requested_;
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
  std::atomic_bool cancel_{false};
  std::atomic<size_t> received_;
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

void streamThroughput(unsigned, size_t items) {
  std::unique_ptr<RSocketServer> server;
  std::shared_ptr<RSocketClient> client;
  yarpl::Reference<BM_Subscriber> subscriber;

  BENCHMARK_SUSPEND {
    LOG(INFO) << "  Running with " << items << " items";

    folly::SocketAddress address{FLAGS_host, static_cast<uint16_t>(FLAGS_port),
                                 true /* allowNameLookup */};
    server = makeServer(std::move(address));

    folly::SocketAddress actual{
        FLAGS_host, *server->listeningPort(), true /* allowNameLookup */};
    client = makeClient(std::move(actual));

    subscriber = yarpl::make_ref<BM_Subscriber>(items);
  }

  client->getRequester()
      ->requestStream(Payload("BM_Stream"))
      ->subscribe(subscriber);

  while (subscriber->received() < items) {
    std::this_thread::yield();
  }

  BENCHMARK_SUSPEND {
    subscriber->cancel();
    subscriber->awaitTerminalEvent();
  }
}

BENCHMARK_PARAM(streamThroughput, 10000)
BENCHMARK_PARAM(streamThroughput, 100000)
BENCHMARK_PARAM(streamThroughput, 1000000)

int main(int argc, char** argv) {
  folly::init(&argc, &argv);

  FLAGS_logtostderr = true;

  LOG(INFO) << "Starting benchmarks...";
  folly::runBenchmarks();

  return 0;
}
