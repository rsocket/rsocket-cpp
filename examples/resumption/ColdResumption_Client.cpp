// Copyright 2004-present Facebook. All Rights Reserved.

#include <iostream>

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"

#include "rsocket/internal/InMemResumeManager.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

using namespace rsocket;
using namespace yarpl;
using namespace yarpl::flowable;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

typedef std::map<std::string, Reference<Subscriber<Payload>>> HelloSubscribers;

namespace {

class HelloSubscriber : public virtual Refcounted, public Subscriber<Payload> {
 public:
  void request(int n) {
    while (!Subscriber<Payload>::subscription()) {
      std::this_thread::yield();
    }
    Subscriber<Payload>::subscription()->request(n);
  }

  int rcvdCount() const {
    return count_;
  };

 protected:
  void onSubscribe(Reference<Subscription> subscription) noexcept override {
    Subscriber<rsocket::Payload>::onSubscribe(subscription);
  }

  void onNext(Payload) noexcept override {
    count_++;
  }

 private:
  std::atomic<int> count_{0};
};

class HelloResumeHandler : public ColdResumeHandler {
 public:
  explicit HelloResumeHandler(HelloSubscribers subscribers)
      : subscribers_(std::move(subscribers)) {}

  std::string generateStreamToken(const Payload& payload, StreamId, StreamType)
      override {
    auto streamToken =
        payload.data->cloneAsValue().moveToFbString().toStdString();
    VLOG(3) << "Generated token: " << streamToken;
    return streamToken;
  }

  Reference<Subscriber<Payload>> handleRequesterResumeStream(
      std::string streamToken,
      size_t consumerAllowance) override {
    CHECK(subscribers_.find(streamToken) != subscribers_.end());
    LOG(INFO) << "Resuming " << streamToken << " stream with allowance "
              << consumerAllowance;
    return subscribers_[streamToken];
  }

 private:
  HelloSubscribers subscribers_;
};

SetupParameters getSetupParams(ResumeIdentificationToken token) {
  SetupParameters setupParameters;
  setupParameters.resumable = true;
  setupParameters.token = token;
  return setupParameters;
}

std::unique_ptr<TcpConnectionFactory> getConnFactory(
    folly::EventBase* eventBase) {
  folly::SocketAddress address;
  address.setFromHostPort(FLAGS_host, FLAGS_port);
  return std::make_unique<TcpConnectionFactory>(*eventBase, address);
}
}

// There are three sessions and three streams.
// There is cold-resumption between the three sessions.
// The first stream lasts through all three sessions.
// The second stream lasts through the second and third session.
// the third stream lives only in the third session.

int main(int argc, char* argv[]) {
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = 0;
  folly::init(&argc, &argv);

  folly::ScopedEventBaseThread worker;

  auto token = ResumeIdentificationToken::generateNew();
  auto resumeManager =
      std::make_shared<InMemResumeManager>(RSocketStats::noop());

  std::string firstPayload = "First";
  std::string secondPayload = "Second";
  std::string thirdPayload = "Third";

  {
    auto firstSub = yarpl::make_ref<HelloSubscriber>();
    auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
        HelloSubscribers({{firstPayload, firstSub}}));
    auto firstClient = RSocket::createConnectedClient(
                           getConnFactory(worker.getEventBase()),
                           getSetupParams(token),
                           nullptr, // responder
                           nullptr, // keepAliveTimer
                           nullptr, // stats
                           nullptr, // connectionEvents
                           resumeManager,
                           coldResumeHandler)
                           .get();
    firstClient->getRequester()
        ->requestStream(Payload(firstPayload))
        ->subscribe(firstSub);
    firstSub->request(7);
    while (firstSub->rcvdCount() < 3) {
      std::this_thread::yield();
    }
    firstClient->disconnect(std::runtime_error("disconnect from client"));
  }

  LOG(INFO) << "============== First Cold Resumption ================";

  {
    auto firstSub = yarpl::make_ref<HelloSubscriber>();
    auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
        HelloSubscribers({{firstPayload, firstSub}}));
    auto secondClient = RSocket::createResumedClient(
                            getConnFactory(worker.getEventBase()),
                            getSetupParams(token),
                            resumeManager,
                            coldResumeHandler)
                            .get();

    firstSub->request(3);

    // Create another stream to verify StreamIds are set properly after
    // resumption
    auto secondSub = yarpl::make_ref<HelloSubscriber>();
    secondClient->getRequester()
        ->requestStream(Payload(secondPayload))
        ->subscribe(secondSub);
    secondSub->request(5);
    firstSub->request(4);
    while (secondSub->rcvdCount() < 1) {
      std::this_thread::yield();
    }
  }

  LOG(INFO) << "============== Second Cold Resumption ================";

  {
    auto firstSub = yarpl::make_ref<HelloSubscriber>();
    auto secondSub = yarpl::make_ref<HelloSubscriber>();
    auto coldResumeHandler =
        std::make_shared<HelloResumeHandler>(HelloSubscribers(
            {{firstPayload, firstSub}, {secondPayload, secondSub}}));
    auto thirdClient = RSocket::createResumedClient(
                           getConnFactory(worker.getEventBase()),
                           getSetupParams(token),
                           resumeManager,
                           coldResumeHandler)
                           .get();

    firstSub->request(6);
    secondSub->request(5);

    // Create another stream to verify StreamIds are set properly after
    // resumption
    auto thirdSub = yarpl::make_ref<HelloSubscriber>();
    thirdClient->getRequester()
        ->requestStream(Payload(thirdPayload))
        ->subscribe(thirdSub);
    thirdSub->request(5);

    getchar();
  }

  return 0;
}
