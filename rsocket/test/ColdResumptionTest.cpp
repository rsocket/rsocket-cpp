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

#include <gtest/gtest.h>

#include <folly/Conv.h>
#include <folly/Format.h>
#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/portability/GFlags.h>

#include "RSocketTests.h"

#include "rsocket/test/handlers/HelloServiceHandler.h"
#include "rsocket/test/test_utils/ColdResumeManager.h"

DEFINE_int32(num_clients, 5, "Number of clients to parallely cold-resume");

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;
using namespace yarpl::flowable;

typedef std::map<std::string, std::shared_ptr<Subscriber<Payload>>>
    HelloSubscribers;

namespace {
class HelloSubscriber : public BaseSubscriber<Payload> {
 public:
  explicit HelloSubscriber(size_t latestValue) : latestValue_(latestValue) {}

  void requestWhenSubscribed(int n) {
    subscribedBaton_.wait();
    this->request(n);
  }

  void awaitLatestValue(size_t value) {
    auto count = 50;
    while (value != latestValue_ && count > 0) {
      VLOG(1) << "Waiting " << count << " ticks for latest value...";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      count--;
      std::this_thread::yield();
    }
    EXPECT_EQ(value, latestValue_);
  }

  size_t valueCount() const {
    return count_;
  }

  size_t getLatestValue() const {
    return latestValue_;
  }

 protected:
  void onSubscribeImpl() noexcept override {
    subscribedBaton_.post();
  }

  void onNextImpl(Payload p) noexcept override {
    auto currValue = folly::to<size_t>(p.data->moveToFbString().toStdString());
    EXPECT_EQ(latestValue_, currValue - 1);
    latestValue_ = currValue;
    count_++;
  }

  void onCompleteImpl() override {}
  void onErrorImpl(folly::exception_wrapper) override {}

 private:
  std::atomic<size_t> latestValue_;
  std::atomic<size_t> count_{0};
  folly::Baton<> subscribedBaton_;
};

class HelloResumeHandler : public ColdResumeHandler {
 public:
  explicit HelloResumeHandler(HelloSubscribers subscribers)
      : subscribers_(std::move(subscribers)) {}

  std::string generateStreamToken(const Payload& payload, StreamId, StreamType)
      const override {
    const auto streamToken =
        payload.data->cloneAsValue().moveToFbString().toStdString();
    VLOG(3) << "Generated token: " << streamToken;
    return streamToken;
  }

  std::shared_ptr<Subscriber<Payload>> handleRequesterResumeStream(
      std::string streamToken,
      size_t consumerAllowance) override {
    CHECK(subscribers_.find(streamToken) != subscribers_.end());
    VLOG(1) << "Resuming " << streamToken << " stream with allowance "
            << consumerAllowance;
    return subscribers_[streamToken];
  }

 private:
  HelloSubscribers subscribers_;
};
} // namespace

std::unique_ptr<rsocket::RSocketClient> createResumedClient(
    folly::EventBase* evb,
    uint32_t port,
    ResumeIdentificationToken token,
    std::shared_ptr<ResumeManager> resumeManager,
    std::shared_ptr<ColdResumeHandler> coldResumeHandler,
    folly::EventBase* stateMachineEvb = nullptr) {
  auto retries = 10;
  while (true) {
    try {
      return RSocket::createResumedClient(
                 getConnFactory(evb, port),
                 token,
                 resumeManager,
                 coldResumeHandler,
                 nullptr, /* responder */
                 kDefaultKeepaliveInterval,
                 nullptr, /* stats */
                 nullptr, /* connectionEvents */
                 ProtocolVersion::Latest,
                 stateMachineEvb)
          .get();
    } catch (const RSocketException& ex) {
      retries--;
      VLOG(1) << "Creation of resumed client failed. Exception " << ex.what()
              << ". Retries Left: " << retries;
      if (retries <= 0) {
        throw ex;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
}

// There are three sessions and three streams.
// There is cold-resumption between the three sessions.
//
// The first stream lasts through all three sessions.
// The second stream lasts through the second and third session.
// The third stream lives only in the third session.
//
// The first stream requests 10 frames
// The second stream requests 10 frames
// The third stream requests 5 frames
void coldResumer(uint32_t port, uint32_t client_num) {
  auto firstPayload = folly::sformat("client{}_first", client_num);
  auto secondPayload = folly::sformat("client{}_second", client_num);
  auto thirdPayload = folly::sformat("client{}_third", client_num);
  size_t firstLatestValue, secondLatestValue;

  folly::ScopedEventBaseThread worker;
  auto token = ResumeIdentificationToken::generateNew();
  auto resumeManager =
      std::make_shared<ColdResumeManager>(RSocketStats::noop());
  {
    auto firstSub = std::make_shared<HelloSubscriber>(0);
    {
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{firstPayload, firstSub}}));
      std::shared_ptr<RSocketClient> firstClient;
      EXPECT_NO_THROW(
          firstClient = makeColdResumableClient(
              worker.getEventBase(),
              port,
              token,
              resumeManager,
              coldResumeHandler));
      firstClient->getRequester()
          ->requestStream(Payload(firstPayload))
          ->subscribe(firstSub);
      firstSub->requestWhenSubscribed(4);
      // Ensure reception of few frames before resuming.
      while (firstSub->valueCount() < 1) {
        std::this_thread::yield();
      }
    }
    worker.getEventBase()->runInEventBaseThreadAndWait(
        [client_num, &firstLatestValue, firstSub = std::move(firstSub)]() {
          firstLatestValue = firstSub->getLatestValue();
          VLOG(1) << folly::sformat(
              "client{} {}", client_num, firstLatestValue);
          VLOG(1) << folly::sformat("client{} First Resume", client_num);
        });
  }

  {
    auto firstSub = std::make_shared<HelloSubscriber>(firstLatestValue);
    auto secondSub = std::make_shared<HelloSubscriber>(0);
    {
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{firstPayload, firstSub}}));
      std::shared_ptr<RSocketClient> secondClient;
      EXPECT_NO_THROW(
          secondClient = createResumedClient(
              worker.getEventBase(),
              port,
              token,
              resumeManager,
              coldResumeHandler));

      // Create another stream to verify StreamIds are set properly after
      // resumption
      secondClient->getRequester()
          ->requestStream(Payload(secondPayload))
          ->subscribe(secondSub);
      firstSub->requestWhenSubscribed(3);
      secondSub->requestWhenSubscribed(5);
      // Ensure reception of few frames before resuming.
      while (secondSub->valueCount() < 1) {
        std::this_thread::yield();
      }
    }
    worker.getEventBase()->runInEventBaseThreadAndWait(
        [client_num,
         &firstLatestValue,
         firstSub = std::move(firstSub),
         &secondLatestValue,
         secondSub = std::move(secondSub)]() {
          firstLatestValue = firstSub->getLatestValue();
          secondLatestValue = secondSub->getLatestValue();
          VLOG(1) << folly::sformat(
              "client{} {}", client_num, firstLatestValue);
          VLOG(1) << folly::sformat(
              "client{} {}", client_num, secondLatestValue);
          VLOG(1) << folly::sformat("client{} Second Resume", client_num);
        });
  }

  {
    auto firstSub = std::make_shared<HelloSubscriber>(firstLatestValue);
    auto secondSub = std::make_shared<HelloSubscriber>(secondLatestValue);
    auto thirdSub = std::make_shared<HelloSubscriber>(0);
    auto coldResumeHandler =
        std::make_shared<HelloResumeHandler>(HelloSubscribers(
            {{firstPayload, firstSub}, {secondPayload, secondSub}}));
    std::shared_ptr<RSocketClient> thirdClient;

    EXPECT_NO_THROW(
        thirdClient = createResumedClient(
            worker.getEventBase(),
            port,
            token,
            resumeManager,
            coldResumeHandler));

    // Create another stream to verify StreamIds are set properly after
    // resumption
    thirdClient->getRequester()
        ->requestStream(Payload(thirdPayload))
        ->subscribe(thirdSub);
    firstSub->requestWhenSubscribed(3);
    secondSub->requestWhenSubscribed(5);
    thirdSub->requestWhenSubscribed(5);

    firstSub->awaitLatestValue(10);
    secondSub->awaitLatestValue(10);
    thirdSub->awaitLatestValue(5);
  }
}

TEST(ColdResumptionTest, DISABLED_SuccessfulResumption) {
  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());
  auto port = *server->listeningPort();

  std::vector<std::thread> clients;

  for (int i = 0; i < FLAGS_num_clients; i++) {
    auto client = std::thread([port, i]() { coldResumer(port, i); });
    clients.push_back(std::move(client));
  }

  for (auto& client : clients) {
    client.join();
  }
}

TEST(ColdResumptionTest, DifferentEvb) {
  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());
  auto port = *server->listeningPort();

  auto payload = "InitialPayload";
  size_t latestValue;

  folly::ScopedEventBaseThread transportWorker{"transportWorker"};
  folly::ScopedEventBaseThread SMWorker{"SMWorker"};

  auto token = ResumeIdentificationToken::generateNew();
  auto resumeManager =
      std::make_shared<ColdResumeManager>(RSocketStats::noop());
  {
    auto firstSub = std::make_shared<HelloSubscriber>(0);
    {
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{payload, firstSub}}));
      std::shared_ptr<RSocketClient> firstClient;
      EXPECT_NO_THROW(
          firstClient = makeColdResumableClient(
              transportWorker.getEventBase(),
              port,
              token,
              resumeManager,
              coldResumeHandler,
              SMWorker.getEventBase()));
      firstClient->getRequester()
          ->requestStream(Payload(payload))
          ->subscribe(firstSub);
      firstSub->requestWhenSubscribed(7);
      // Ensure reception of few frames before resuming.
      while (firstSub->valueCount() < 1) {
        std::this_thread::yield();
      }
    }
    SMWorker.getEventBase()->runInEventBaseThreadAndWait(
        [&latestValue, firstSub = std::move(firstSub)]() {
          latestValue = firstSub->getLatestValue();
          VLOG(1) << latestValue;
          VLOG(1) << "First Resume";
        });
  }

  {
    auto firstSub = std::make_shared<HelloSubscriber>(latestValue);
    {
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{payload, firstSub}}));
      std::shared_ptr<RSocketClient> secondClient;
      EXPECT_NO_THROW(
          secondClient = createResumedClient(
              transportWorker.getEventBase(),
              port,
              token,
              resumeManager,
              coldResumeHandler,
              SMWorker.getEventBase()));

      firstSub->requestWhenSubscribed(3);
      // Ensure reception of few frames before resuming.
      while (firstSub->valueCount() < 1) {
        std::this_thread::yield();
      }
      firstSub->awaitLatestValue(10);
    }
  }

  server->shutdownAndWait();
}

// Attempt a resumption when the previous transport/client hasn't
// disconnected it.  Verify resumption succeeds after the previous
// transport is disconnected.
TEST(ColdResumptionTest, DisconnectResumption) {
  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());
  auto port = *server->listeningPort();

  auto payload = "InitialPayload";

  folly::ScopedEventBaseThread transportWorker{"transportWorker"};

  auto token = ResumeIdentificationToken::generateNew();
  auto resumeManager =
      std::make_shared<ColdResumeManager>(RSocketStats::noop());
  auto sub = std::make_shared<HelloSubscriber>(0);
  auto crh =
      std::make_shared<HelloResumeHandler>(HelloSubscribers({{payload, sub}}));
  std::shared_ptr<RSocketClient> client;
  EXPECT_NO_THROW(
      client = makeColdResumableClient(
          transportWorker.getEventBase(), port, token, resumeManager, crh));
  client->getRequester()->requestStream(Payload(payload))->subscribe(sub);
  sub->requestWhenSubscribed(7);
  // Ensure reception of few frames before resuming.
  while (sub->valueCount() < 7) {
    std::this_thread::yield();
  }

  auto resumedSub = std::make_shared<HelloSubscriber>(7);
  auto resumedCrh = std::make_shared<HelloResumeHandler>(
      HelloSubscribers({{payload, resumedSub}}));

  std::shared_ptr<RSocketClient> resumedClient;
  EXPECT_NO_THROW(
      resumedClient = createResumedClient(
          transportWorker.getEventBase(),
          port,
          token,
          resumeManager,
          resumedCrh));

  resumedSub->requestWhenSubscribed(3);
  resumedSub->awaitLatestValue(10);

  server->shutdownAndWait();
}
