// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>

#include <folly/init/Init.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "rsocket/RSocket.h"

#include "rsocket/test/test_utils/ColdResumeManager.h"
#include "rsocket/transports/tcp/TcpConnectionFactory.h"

using namespace rsocket;
using namespace yarpl::flowable;

DEFINE_string(host, "localhost", "host to connect to");
DEFINE_int32(port, 9898, "host:port to connect to");

typedef std::map<std::string, std::shared_ptr<Subscriber<Payload>>>
    HelloSubscribers;

namespace {

class HelloSubscriber : public Subscriber<Payload> {
 public:
  void request(int n) {
    while (!subscription_) {
      std::this_thread::yield();
    }
    subscription_->request(n);
  }

  int rcvdCount() const {
    return count_;
  };

 protected:
  void onSubscribe(std::shared_ptr<Subscription> subscription) override {
    subscription_ = subscription;
  }

  void onNext(Payload) noexcept override {
    count_++;
  }

  void onComplete() override {}
  void onError(folly::exception_wrapper) override {}

 private:
  std::shared_ptr<Subscription> subscription_;
  std::atomic<int> count_{0};
};

class HelloResumeHandler : public ColdResumeHandler {
 public:
  explicit HelloResumeHandler(HelloSubscribers subscribers)
      : subscribers_(std::move(subscribers)) {}

  std::string generateStreamToken(const Payload& payload, StreamId, StreamType)
      const override {
    auto streamToken =
        payload.data->cloneAsValue().moveToFbString().toStdString();
    VLOG(3) << "Generated token: " << streamToken;
    return streamToken;
  }

  std::shared_ptr<Subscriber<Payload>> handleRequesterResumeStream(
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
} // namespace

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

  std::string firstPayload = "First";
  std::string secondPayload = "Second";
  std::string thirdPayload = "Third";

  {
    auto resumeManager = std::make_shared<ColdResumeManager>(
        RSocketStats::noop(), "" /* inputFile */);
    {
      auto firstSub = std::make_shared<HelloSubscriber>();
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{firstPayload, firstSub}}));
      auto firstClient = RSocket::createConnectedClient(
                             getConnFactory(worker.getEventBase()),
                             getSetupParams(token),
                             nullptr, // responder
                             kDefaultKeepaliveInterval,
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
    worker.getEventBase()->runInEventBaseThreadAndWait(
        [resumeManager = std::move(resumeManager)]() {
          // We want to persist state after RSocketStateMachine of the client
          // has been completely destroyed and before we start the next scope.
          // Since the RSocketStateMachine's destruction proceeds
          // asynchronously in worker thread, we have to schedule the
          // persistence in the worker thread.
          resumeManager->persistState("/tmp/firstResumption.json");
        });
  }

  LOG(INFO) << "============== First Cold Resumption ================";

  {
    auto resumeManager = std::make_shared<ColdResumeManager>(
        RSocketStats::noop(), "/tmp/firstResumption.json" /* inputFile */);
    {
      auto firstSub = std::make_shared<HelloSubscriber>();
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{firstPayload, firstSub}}));
      auto secondClient = RSocket::createResumedClient(
                              getConnFactory(worker.getEventBase()),
                              token,
                              resumeManager,
                              coldResumeHandler)
                              .get();

      firstSub->request(3);

      // Create another stream to verify StreamIds are set properly after
      // resumption
      auto secondSub = std::make_shared<HelloSubscriber>();
      secondClient->getRequester()
          ->requestStream(Payload(secondPayload))
          ->subscribe(secondSub);
      secondSub->request(5);
      firstSub->request(4);
      while (secondSub->rcvdCount() < 1) {
        std::this_thread::yield();
      }
    }
    worker.getEventBase()->runInEventBaseThreadAndWait(
        [resumeManager = std::move(resumeManager)]() {
          // Refer to comments in the above scope.
          resumeManager->persistState("/tmp/secondResumption.json");
        });
  }

  LOG(INFO) << "============== Second Cold Resumption ================";

  {
    auto resumeManager = std::make_shared<ColdResumeManager>(
        RSocketStats::noop(), "/tmp/secondResumption.json" /* inputFile */);
    auto firstSub = std::make_shared<HelloSubscriber>();
    auto secondSub = std::make_shared<HelloSubscriber>();
    auto coldResumeHandler =
        std::make_shared<HelloResumeHandler>(HelloSubscribers(
            {{firstPayload, firstSub}, {secondPayload, secondSub}}));
    auto thirdClient = RSocket::createResumedClient(
                           getConnFactory(worker.getEventBase()),
                           token,
                           resumeManager,
                           coldResumeHandler)
                           .get();

    firstSub->request(6);
    secondSub->request(5);

    // Create another stream to verify StreamIds are set properly after
    // resumption
    auto thirdSub = std::make_shared<HelloSubscriber>();
    thirdClient->getRequester()
        ->requestStream(Payload(thirdPayload))
        ->subscribe(thirdSub);
    thirdSub->request(5);

    getchar();
  }

  return 0;
}
