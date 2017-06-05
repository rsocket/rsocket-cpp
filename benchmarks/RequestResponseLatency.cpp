// Copyright 2004-present Facebook. All Rights Reserved.

#include <benchmark/benchmark.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <src/transports/tcp/TcpConnectionAcceptor.h>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <gflags/gflags.h>
#include "src/RSocket.h"
#include "src/transports/tcp/TcpConnectionFactory.h"
#include "yarpl/Flowable.h"
#include "yarpl/utils/ExceptionString.h"

using namespace ::folly;
using namespace ::rsocket;
using namespace yarpl;

#define MESSAGE_LENGTH (32)

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

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
    LOG(INFO) << "requested=" << n;

    if (cancelled_) {
      LOG(INFO) << "emission stopped by cancellation";
      return;
    }

    subscriber_->onNext(Payload(data_));
    subscriber_->onComplete();
  }

  void cancel() noexcept override {
    LOG(INFO) << "cancellation received";
    cancelled_ = true;
  }

  yarpl::Reference<yarpl::flowable::Subscriber<Payload>> subscriber_;
  std::string data_;
  std::atomic_bool cancelled_;
};

class BM_RequestHandler : public RSocketResponder {
 public:
  // TODO(lehecka): enable when we have support for request-response
  yarpl::Reference<yarpl::flowable::Flowable<Payload>> handleRequestStream(
      Payload request,
      StreamId streamId) override {
    CHECK(false) << "not implemented";
  }

  // void handleRequestResponse(
  //     Payload request, StreamId streamId, const
  //     yarpl::Reference<yarpl::flowable::Subscriber<Payload>> &response)
  //     noexcept override
  // {
  //     LOG(INFO) << "BM_RequestHandler.handleRequestResponse " << request;

  //     response->onSubscribe(
  //         std::make_shared<BM_Subscription>(response, MESSAGE_LENGTH));
  // }

  // std::shared_ptr<StreamState> handleSetupPayload(
  //     ReactiveSocket &socket, ConnectionSetupPayload request) noexcept
  //     override
  // {
  //     LOG(INFO) << "BM_RequestHandler.handleSetupPayload " << request;
  //     return nullptr;
  // }
};

class BM_Subscriber : public yarpl::flowable::Subscriber<Payload> {
 public:
  ~BM_Subscriber() {
    LOG(INFO) << "BM_Subscriber destroy " << this;
  }

  BM_Subscriber()
      : initialRequest_(8), thresholdForRequest_(initialRequest_ * 0.75) {
    LOG(INFO) << "BM_Subscriber " << this << " created with => "
              << "  Initial Request: " << initialRequest_
              << "  Threshold for re-request: " << thresholdForRequest_;
  }

  void onSubscribe(yarpl::Reference<yarpl::flowable::Subscription>
                       subscription) noexcept override {
    LOG(INFO) << "BM_Subscriber " << this << " onSubscribe";
    subscription_ = std::move(subscription);
    requested_ = initialRequest_;
    subscription_->request(initialRequest_);
  }

  void onNext(Payload element) noexcept override {
    LOG(INFO) << "BM_Subscriber " << this
              << " onNext as string: " << element.moveDataToString();

    if (--requested_ == thresholdForRequest_) {
      int toRequest = (initialRequest_ - thresholdForRequest_);
      LOG(INFO) << "BM_Subscriber " << this << " requesting " << toRequest
                << " more items";
      requested_ += toRequest;
      subscription_->request(toRequest);
    }
  }

  void onComplete() noexcept override {
    LOG(INFO) << "BM_Subscriber " << this << " onComplete";
    terminated_ = true;
    completed_ = true;
    terminalEventCV_.notify_all();
  }

  void onError(std::exception_ptr ex) noexcept override {
    LOG(INFO) << "BM_Subscriber " << this << " onError: "
              << yarpl::exceptionStr(ex);
    terminated_ = true;
    terminalEventCV_.notify_all();
  }

  void awaitTerminalEvent() {
    LOG(INFO) << "BM_Subscriber " << this << " block thread";
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);
    // if shutdown gets implemented this would then be released by it
    terminalEventCV_.wait(lk, [this] { return terminated_; });
    LOG(INFO) << "BM_Subscriber " << this << " unblocked";
  }

  bool completed() {
    return completed_;
  }

 private:
  int initialRequest_;
  int thresholdForRequest_;
  int requested_;
  yarpl::Reference<yarpl::flowable::Subscription> subscription_;
  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
  std::atomic_bool completed_{false};
};

class BM_RsFixture : public benchmark::Fixture {
 public:
  BM_RsFixture()
      : host_(FLAGS_host),
        port_(static_cast<uint16_t>(FLAGS_port)),
        serverRs_(RSocket::createServer(std::make_unique<TcpConnectionAcceptor>(
            TcpConnectionAcceptor::Options(port_)))),
        handler_(std::make_shared<BM_RequestHandler>()) {
    FLAGS_v = 0;
    FLAGS_minloglevel = 6;
    serverRs_->start([this](auto& setupParams) { return handler_; });
  }

  virtual ~BM_RsFixture() {}

  void SetUp(const benchmark::State& state) noexcept override {}

  void TearDown(const benchmark::State& state) noexcept override {}

  std::string host_;
  uint16_t port_;
  std::unique_ptr<RSocketServer> serverRs_;
  std::shared_ptr<BM_RequestHandler> handler_;
};

BENCHMARK_F(BM_RsFixture, BM_RequestResponse_Latency)(benchmark::State& state) {
  // TODO(lehecka): enable test
  //    folly::SocketAddress address;
  //    address.setFromHostPort(host_, port_);
  //
  //    auto clientRs =
  //    RSocket::createClient(std::make_unique<TcpConnectionFactory>(
  //        std::move(address)));
  //    int reqs = 0;
  //
  //    auto rs = clientRs->connect().get();
  //
  //    while (state.KeepRunning())
  //    {
  //        auto sub = make_ref<BM_Subscriber>();
  //        rs->requestResponse(Payload("BM_RequestResponse"))->subscribe(sub);
  //
  //        while (!sub->completed())
  //        {
  //            std::this_thread::yield();
  //        }
  //
  //        reqs++;
  //    }
  //
  //    char label[256];
  //
  //    std::snprintf(label, sizeof(label), "Message Length: %d",
  //    MESSAGE_LENGTH);
  //    state.SetLabel(label);
  //
  //    state.SetItemsProcessed(reqs);
}

BENCHMARK_MAIN()
