// Copyright 2004-present Facebook. All Rights Reserved.

#include <benchmark/benchmark.h>
#include <thread>

#include <folly/io/async/ScopedEventBaseThread.h>
#include <iostream>
#include <experimental/rsocket/transports/TcpConnectionAcceptor.h>
#include <src/NullRequestHandler.h>
#include <src/SubscriptionBase.h>
#include "rsocket/RSocket.h"
#include "rsocket/OldNewBridge.h"
#include "rsocket/transports/TcpConnectionFactory.h"
#include "yarpl/Flowables.h"

using namespace ::reactivesocket;
using namespace ::folly;
using namespace ::rsocket;

#define MESSAGE_LENGTH (32)

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

class BM_Subscription : public SubscriptionBase {
public:
    explicit BM_Subscription(
        std::shared_ptr<Subscriber<Payload>> subscriber,
        size_t length)
        : ExecutorBase(defaultExecutor()),
        subscriber_(std::move(subscriber)),
        data_(length, 'a'),
        cancelled_(false)
    {
    }

private:
    void requestImpl(size_t n) noexcept override
    {
        LOG(INFO) << "requested=" << n << " currentElem=" << currentElem_;

        for (size_t i = 0; i < n; i++) {
            if (cancelled_) {
                LOG(INFO) << "emission stopped by cancellation";
                return;
            }
            subscriber_->onNext(Payload(data_));
            currentElem_++;
        }
    }

    void cancelImpl() noexcept override
    {
        LOG(INFO) << "cancellation received";
        cancelled_ = true;
    }

    std::shared_ptr<Subscriber<Payload>> subscriber_;
    std::string data_;
    size_t currentElem_ = 0;
    std::atomic_bool cancelled_;
};

class BM_RequestHandler : public RSocketRequestHandler
{
public:
    yarpl::Reference<yarpl::flowable::Flowable<reactivesocket::Payload>>
    handleRequestStream(
      reactivesocket::Payload request,
      reactivesocket::StreamId streamId) override {
        CHECK(false) << "not implemented";
        // TODO(lehecka) need to implement new operator fromGenerator
        // return yarpl::flowable::Flowables::fromGenerator<reactivesocket::Payload>(
        //     []{return Payload(std::string(MESSAGE_LENGTH, 'a')); });
    }
};

class BM_Subscriber
    : public reactivesocket::Subscriber<reactivesocket::Payload> {
public:
    ~BM_Subscriber()
    {
        LOG(INFO) << "BM_Subscriber destroy " << this;
    }

    BM_Subscriber(int initialRequest)
        : initialRequest_(initialRequest),
        thresholdForRequest_(initialRequest * 0.75),
        received_(0)
    {
        LOG(INFO) << "BM_Subscriber " << this << " created with => "
            << "  Initial Request: " << initialRequest
            << "  Threshold for re-request: " << thresholdForRequest_;
    }

    void onSubscribe(std::shared_ptr<reactivesocket::Subscription> subscription) noexcept override
    {
        LOG(INFO) << "BM_Subscriber " << this << " onSubscribe";
        subscription_ = std::move(subscription);
        requested_ = initialRequest_;
        subscription_->request(initialRequest_);
    }

    void onNext(reactivesocket::Payload element) noexcept override
    {
        LOG(INFO) << "BM_Subscriber " << this
            << " onNext as string: " << element.moveDataToString();

        received_.store(received_ + 1, std::memory_order_release);

        if (--requested_ == thresholdForRequest_) {
            int toRequest = (initialRequest_ - thresholdForRequest_);
            LOG(INFO) << "BM_Subscriber " << this << " requesting " << toRequest
                << " more items";
            requested_ += toRequest;
            subscription_->request(toRequest);
        };

        if (cancel_)
        {
            subscription_->cancel();
        }
    }

    void onComplete() noexcept override
    {
        LOG(INFO) << "BM_Subscriber " << this << " onComplete";
        terminated_ = true;
        terminalEventCV_.notify_all();
    }

    void onError(folly::exception_wrapper ex) noexcept override
    {
        LOG(INFO) << "BM_Subscriber " << this << " onError " << ex.what();
        terminated_ = true;
        terminalEventCV_.notify_all();
    }

    void awaitTerminalEvent()
    {
        LOG(INFO) << "BM_Subscriber " << this << " block thread";
        // now block this thread
        std::unique_lock<std::mutex> lk(m_);
        // if shutdown gets implemented this would then be released by it
        terminalEventCV_.wait(lk, [this] { return terminated_; });
        LOG(INFO) << "BM_Subscriber " << this << " unblocked";
    }

    void cancel()
    {
        cancel_ = true;
    }

    size_t received()
    {
        return received_.load(std::memory_order_acquire);
    }

private:
    int initialRequest_;
    int thresholdForRequest_;
    int requested_;
    std::shared_ptr<reactivesocket::Subscription> subscription_;
    bool terminated_{false};
    std::mutex m_;
    std::condition_variable terminalEventCV_;
    std::atomic_bool cancel_{false};
    std::atomic<size_t> received_;
};

class BM_RsFixture : public benchmark::Fixture
{
public:
    BM_RsFixture() :
        host_(FLAGS_host),
        port_(static_cast<uint16_t>(FLAGS_port)),
        serverRs_(RSocket::createServer(std::make_unique<TcpConnectionAcceptor>(
            TcpConnectionAcceptor::Options{port_}))),
        handler_(std::make_shared<BM_RequestHandler>())
    {
        FLAGS_minloglevel = 100;
        serverRs_->start([this](auto r) { return handler_; });
    }

    virtual ~BM_RsFixture()
    {
    }

    void SetUp(benchmark::State &state) noexcept override
    {
    }

    void TearDown(benchmark::State &state) noexcept override
    {
    }

    std::string host_;
    uint16_t port_;
    std::unique_ptr<RSocketServer> serverRs_;
    std::shared_ptr<BM_RequestHandler> handler_;
};

BENCHMARK_DEFINE_F(BM_RsFixture, BM_Stream_Throughput)(benchmark::State &state)
{
    folly::SocketAddress address;
    address.setFromHostPort(host_, port_);

    auto clientRs = RSocket::createClient(std::make_unique<TcpConnectionFactory>(
        std::move(address)));

    auto s = std::make_shared<BM_Subscriber>(state.range(0));

    clientRs
        ->connect()
            .then(
                [s](std::shared_ptr<RSocketRequester> rs)
                {
                   rs->requestStream(Payload("BM_Stream"))->subscribe(
                        yarpl::Reference<yarpl::flowable::Subscriber<Payload>>(
                            new NewToOldSubscriber(s)));
                });

    while (state.KeepRunning())
    {
        std::this_thread::yield();
    }

    size_t rcved = s->received();

    s->cancel();
    s->awaitTerminalEvent();

    char label[256];

    std::snprintf(label, sizeof(label), "Message Length: %d", MESSAGE_LENGTH);
    state.SetLabel(label);

    state.SetItemsProcessed(rcved);
}

BENCHMARK_REGISTER_F(BM_RsFixture, BM_Stream_Throughput)->Arg(8)->Arg(32)->Arg(128);

BENCHMARK_MAIN()
