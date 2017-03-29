// Copyright 2004-present Facebook. All Rights Reserved.

#include <benchmark/benchmark.h>
#include <thread>

#include <folly/io/async/ScopedEventBaseThread.h>
#include <iostream>
#include <experimental/rsocket/transports/TcpConnectionAcceptor.h>
#include <src/NullRequestHandler.h>
#include <src/SubscriptionBase.h>
#include "rsocket/RSocket.h"
#include "rsocket/transports/TcpConnectionFactory.h"

using namespace ::reactivesocket;
using namespace ::folly;
using namespace ::rsocket;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

class BM_Subscription : public SubscriptionBase {
public:
    explicit BM_Subscription(
        std::shared_ptr<Subscriber<Payload>> subscriber,
        std::string name)
        : ExecutorBase(defaultExecutor()),
        subscriber_(std::move(subscriber)),
        name_(std::move(name)),
        cancelled_(false)
    {
    }

private:
    void requestImpl(size_t n) noexcept override
    {
        LOG(INFO) << "requested=" << n << " currentElem=" << currentElem_;

        if (cancelled_) {
            LOG(INFO) << "emission stopped by cancellation";
            return;
        }

        std::stringstream ss;
        ss << "Hello " << name_ << " " << currentElem_ << "!";
        std::string s = ss.str();
        subscriber_->onNext(Payload(s));
        currentElem_++;
        subscriber_->onComplete();
    }

    void cancelImpl() noexcept override
    {
        LOG(INFO) << "cancellation received";
        // simple cancellation token (nothing to shut down, just stop next loop)
        cancelled_ = true;
    }

    std::shared_ptr<Subscriber<Payload>> subscriber_;
    std::string name_;
    size_t currentElem_ = 0;
    std::atomic_bool cancelled_;
};

class BM_RequestHandler : public DefaultRequestHandler
{
public:
    void handleRequestResponse(
        Payload request, StreamId streamId, const std::shared_ptr<Subscriber<Payload>> &response) noexcept override
    {
        LOG(INFO) << "BM_RequestHandler.handleRequestResponse " << request;

        // string from payload data
        const char* p = reinterpret_cast<const char*>(request.data->data());
        auto requestString = std::string(p, request.data->length());

        response->onSubscribe(
            std::make_shared<BM_Subscription>(response, requestString));
    }

    std::shared_ptr<StreamState> handleSetupPayload(
        ReactiveSocket &socket, ConnectionSetupPayload request) noexcept override
    {
        LOG(INFO) << "BM_RequestHandler.handleSetupPayload " << request;
        // TODO what should this do?
        return nullptr;
    }
};

class BM_Subscriber
    : public reactivesocket::Subscriber<reactivesocket::Payload> {
public:
    ~BM_Subscriber()
    {
        LOG(INFO) << "BM_Subscriber destroy " << this;
    }

    BM_Subscriber(int initialRequest, int numToTake)
        : initialRequest_(initialRequest),
        thresholdForRequest_(initialRequest * 0.75),
        numToTake_(numToTake),
        received_(0)
    {
        LOG(INFO) << "BM_Subscriber " << this << " created with => "
            << "  Initial Request: " << initialRequest
            << "  Threshold for re-request: " << thresholdForRequest_
            << "  Num to Take: " << numToTake;
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
        received_++;
        if (--requested_ == thresholdForRequest_) {
            int toRequest = (initialRequest_ - thresholdForRequest_);
            LOG(INFO) << "BM_Subscriber " << this << " requesting " << toRequest
                << " more items";
            requested_ += toRequest;
            subscription_->request(toRequest);
        };

//        if (cancel_)
//        {
//            subscription_->cancel();
//        }
    }

    void onComplete() noexcept override
    {
        LOG(INFO) << "BM_Subscriber " << this << " onComplete";
        terminated_ = true;
        completed_ = true;
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

    bool completed()
    {
        return completed_;
    }

    int received()
    {
        return received_;
    }

private:
    int initialRequest_;
    int thresholdForRequest_;
    int numToTake_;
    int requested_;
    int received_;
    std::shared_ptr<reactivesocket::Subscription> subscription_;
    bool terminated_{false};
    std::mutex m_;
    std::condition_variable terminalEventCV_;
    std::atomic_bool cancel_{false};
    std::atomic_bool completed_{false};
};

class BM_RsFixture : public benchmark::Fixture
{
public:
    BM_RsFixture() :
        host_(FLAGS_host),
        port_(FLAGS_port),
        serverRs_(RSocket::createServer(TcpConnectionAcceptor::create(port_))),
        handler_(std::make_shared<BM_RequestHandler>())
    {
        FLAGS_v = 0;
        FLAGS_minloglevel = 6;
        serverRs_->start([this](auto r) { return handler_; });
    }

    virtual ~BM_RsFixture()
    {
    }

    virtual void SetUp(benchmark::State &state)
    {
    }

    virtual void TearDown(benchmark::State &state)
    {
    }

    std::string host_;
    int port_;
    std::unique_ptr<RSocketServer> serverRs_;
    std::shared_ptr<BM_RequestHandler> handler_;
};

BENCHMARK_DEFINE_F(BM_RsFixture, BM_RequestResponse_Latency)(benchmark::State &state)
{
    auto clientRs = RSocket::createClient(TcpConnectionFactory::create(host_, port_));
    int reqs = 0;

    auto rs = clientRs->connect().get();

    while (state.KeepRunning())
    {
        auto sub = std::make_shared<BM_Subscriber>(5, 6);
        rs->requestResponse(Payload("BM_RequestResponse"), sub);

        while (!sub->completed())
        {
            std::this_thread::yield();
        }

        reqs++;
    }

    //state.SetBytesProcessed(totalBytesReceived);
    state.SetItemsProcessed(reqs);
}

BENCHMARK_REGISTER_F(BM_RsFixture, BM_RequestResponse_Latency)->Arg(2);

BENCHMARK_MAIN()

