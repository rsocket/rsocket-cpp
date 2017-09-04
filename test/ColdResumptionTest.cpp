// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>

#include <folly/Conv.h>
#include <folly/io/async/ScopedEventBaseThread.h>

#include "RSocketTests.h"

#include "test/handlers/HelloServiceHandler.h"
#include "test/test_utils/ColdResumeManager.h"

using namespace rsocket;
using namespace rsocket::tests;
using namespace rsocket::tests::client_server;
using namespace yarpl;
using namespace yarpl::flowable;

typedef std::map<std::string, Reference<Subscriber<Payload>>> HelloSubscribers;

namespace {
class HelloSubscriber : public virtual Refcounted, public Subscriber<Payload> {
 public:
  explicit HelloSubscriber(size_t latestValue) : latestValue_(latestValue) {}

  void request(int n) {
    subscribedBaton_.wait();
    Subscriber<Payload>::subscription()->request(n);
  }

  void awaitLatestValue(size_t value) {
    auto count = 3;
    while (value != latestValue_ && count > 0) {
      VLOG(1) << "Wait for " << count << " seconds for latest value";
      /* sleep override */ sleep(1);
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
  void onSubscribe(Reference<Subscription> subscription) noexcept override {
    Subscriber<rsocket::Payload>::onSubscribe(subscription);
    subscribedBaton_.post();
  }

  void onNext(Payload p) noexcept override {
    auto currValue = folly::to<size_t>(p.data->moveToFbString().toStdString());
    EXPECT_EQ(latestValue_, currValue - 1);
    latestValue_ = currValue;
    count_++;
  }

 private:
  std::atomic<size_t> latestValue_;
  std::atomic<size_t> count_;
  folly::Baton<> subscribedBaton_;
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
    VLOG(1) << "Resuming " << streamToken << " stream with allowance "
            << consumerAllowance;
    return subscribers_[streamToken];
  }

 private:
  HelloSubscribers subscribers_;
};
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

TEST(ColdResumptionTest, SuccessfulResumption) {
  std::string firstPayload = "First";
  std::string secondPayload = "Second";
  std::string thirdPayload = "Third";
  size_t firstLatestValue, secondLatestValue;

  auto server = makeResumableServer(std::make_shared<HelloServiceHandler>());

  folly::ScopedEventBaseThread worker;
  auto token = ResumeIdentificationToken::generateNew();
  auto resumeManager =
      std::make_shared<ColdResumeManager>(RSocketStats::noop());
  {
    auto firstSub = make_ref<HelloSubscriber>(0);
    {
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{firstPayload, firstSub}}));
      std::shared_ptr<RSocketClient> firstClient;
      EXPECT_NO_THROW(
          firstClient = makeColdResumableClient(
              worker.getEventBase(),
              *server->listeningPort(),
              token,
              resumeManager,
              coldResumeHandler));
      firstClient->getRequester()
          ->requestStream(Payload(firstPayload))
          ->subscribe(firstSub);
      firstSub->request(4);
      // Ensure reception of few frames before resuming.
      while (firstSub->valueCount() < 1) {
        std::this_thread::yield();
      }
    }
    firstLatestValue = firstSub->getLatestValue();
  }

  VLOG(1) << "============== First Cold Resumption ================";

  {
    auto firstSub = yarpl::make_ref<HelloSubscriber>(firstLatestValue);
    auto secondSub = yarpl::make_ref<HelloSubscriber>(0);
    {
      auto coldResumeHandler = std::make_shared<HelloResumeHandler>(
          HelloSubscribers({{firstPayload, firstSub}}));
      std::shared_ptr<RSocketClient> secondClient;
      EXPECT_NO_THROW(
          secondClient =
              RSocket::createResumedClient(
                  getConnFactory(
                      worker.getEventBase(), *server->listeningPort()),
                  token,
                  resumeManager,
                  coldResumeHandler)
                  .get());

      // Create another stream to verify StreamIds are set properly after
      // resumption
      secondClient->getRequester()
          ->requestStream(Payload(secondPayload))
          ->subscribe(secondSub);
      firstSub->request(3);
      secondSub->request(5);
      // Ensure reception of few frames before resuming.
      while (secondSub->valueCount() < 1) {
        std::this_thread::yield();
      }
    }
    firstLatestValue = firstSub->getLatestValue();
    secondLatestValue = secondSub->getLatestValue();
  }

  VLOG(1) << "============= Second Cold Resumption ===============";

  {
    auto firstSub = yarpl::make_ref<HelloSubscriber>(firstLatestValue);
    auto secondSub = yarpl::make_ref<HelloSubscriber>(secondLatestValue);
    auto thirdSub = yarpl::make_ref<HelloSubscriber>(0);
    auto coldResumeHandler =
        std::make_shared<HelloResumeHandler>(HelloSubscribers(
            {{firstPayload, firstSub}, {secondPayload, secondSub}}));
    std::shared_ptr<RSocketClient> thirdClient;
    EXPECT_NO_THROW(
        thirdClient =
            RSocket::createResumedClient(
                getConnFactory(worker.getEventBase(), *server->listeningPort()),
                token,
                resumeManager,
                coldResumeHandler)
                .get());

    // Create another stream to verify StreamIds are set properly after
    // resumption
    thirdClient->getRequester()
        ->requestStream(Payload(secondPayload))
        ->subscribe(thirdSub);
    firstSub->request(3);
    secondSub->request(5);
    thirdSub->request(5);

    firstSub->awaitLatestValue(10);
    secondSub->awaitLatestValue(10);
    thirdSub->awaitLatestValue(5);
  }
}
