// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/Tuple.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

/*
 * Verbose usage of interfaces with inline class implementations.
 *
 * - Demonstrating subscribe, onSubscribe, request, onNext, cancel.
 */
TEST(FlowableCreateSubscribe, SubscribeRequestAndCancel) {
  // Subscription that emits integers forever as long as requested
  class InfiniteIntegersSource : public Subscription {
    std::unique_ptr<Subscriber<int>> subscriber_;
    std::atomic_bool isCancelled{false};

   public:
    static void subscribe(std::unique_ptr<Subscriber<int>> subscriber) {
      InfiniteIntegersSource s_(std::move(subscriber));
    }

    void start() {
      subscriber_->onSubscribe(this);
    }

    void cancel() override {
      isCancelled = true;
      // complete downstream like take(...) would
      subscriber_->onComplete();
    }
    void request(int64_t n) override {
      // NOTE: Do not implement real code like this
      // This is NOT safe at all since request(n) can be called concurrently
      // This assumes synchronous execution which this unit test does
      // just to test the most basic wiring of all the types together.
      for (auto i = 1; i < n; i++) {
        if (isCancelled) {
          return;
        }
        subscriber_->onNext(i);
      }
    }

   protected:
    InfiniteIntegersSource(std::unique_ptr<Subscriber<int>> subscriber)
        : subscriber_(std::move(subscriber)) {
      subscriber_->onSubscribe(this);
    }
  };

  // Subscriber that requests 10 items, then cancels after receiving 6
  class MySubscriber : public Subscriber<int> {
    Subscription* subscription;

   public:
    void onNext(const int& value) override {
      std::cout << "received& " << value << std::endl;
      if (value == 6) {
        subscription->cancel();
      }
    }
    void onError(std::exception_ptr e) override {}
    void onComplete() override {}
    void onSubscribe(Subscription* s) override {
      std::cout << "onSubscribe in subscriber" << std::endl;
      subscription = s;
      subscription->request((long)10);
    }
  };

  // create Flowable around InfiniteIntegersSource
  auto a =
      Flowable<int>::create([](std::unique_ptr<Subscriber<int>> subscriber) {
        InfiniteIntegersSource::subscribe(std::move(subscriber));
      });

  auto ts = TestSubscriber<int>::create(std::make_unique<MySubscriber>());
  a->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(6);
}

TEST(FlowableCreateSubscribe, OnError) {
  // Subscription that fails
  class Source : public Subscription {
    std::unique_ptr<Subscriber<int>> subscriber_;

   public:
    Source(std::unique_ptr<Subscriber<int>> subscriber)
        : subscriber_(std::move(subscriber)) {}

    void start() {
      subscriber_->onSubscribe(this);
    }

    void cancel() override {}
    void request(int64_t n) override {
      try {
        // simulate user function throwing and being caught
        throw std::runtime_error("something broke!");
      } catch (const std::exception&) {
        subscriber_->onError(std::current_exception());
      }
    }
  };

  // create Flowable around Source
  auto a =
      Flowable<int>::create([](std::unique_ptr<Subscriber<int>> subscriber) {
        auto s = Source(std::move(subscriber));
        s.start();
      });

  auto ts = TestSubscriber<int>::create();
  a->subscribe(ts->unique_subscriber());
  ts->assertOnErrorMessage("something broke!");
}

/**
 * Assert that all items passed through the Flowable get destroyed
 */
TEST(FlowableCreateSubscribe, ItemsCollectedSynchronously) {
  class Source : public Subscription {
    std::unique_ptr<Subscriber<Tuple>> subscriber_;
    std::atomic_bool isCancelled{false};

   public:
    Source(std::unique_ptr<Subscriber<Tuple>> subscriber)
        : subscriber_(std::move(subscriber)) {}

    void start() {
      subscriber_->onSubscribe(this);
    }

    void cancel() override {
      isCancelled = true;
    }
    void request(int64_t n) override {
      // ignoring n for tests ... DO NOT DO THIS FOR REAL
      subscriber_->onNext(Tuple{1, 2});
      subscriber_->onNext(Tuple{2, 3});
      subscriber_->onNext(Tuple{3, 4});
      subscriber_->onComplete();
    }
  };

  // create Flowable around Source
  auto a = Flowable<Tuple>::create(
      [](std::unique_ptr<Subscriber<Tuple>> subscriber) {
        Source s(std::move(subscriber));
        s.start();
      });

  auto ts = TestSubscriber<Tuple>::create();
  a->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  // reset the TestSubscriber since it holds the onNexted values
  ts.reset();

  std::cout << "Remaining tuples == " << Tuple::instanceCount << std::endl;

  EXPECT_EQ(0, Tuple::instanceCount);
  std::cout << "-----------------------------" << std::endl;
}

/**
 * Assert that memory is not retained when a single Flowable
 * is subscribed to multiple times.
 */
TEST(
    FlowableCreateSubscribe,
    TestRetainOnStaticAsyncFlowableWithMultipleSubscribers) {
  static std::atomic<int> subscriptionInstanceCount;
  static std::atomic<int> subscriberInstanceCount;

  class MySubscriber : public Subscriber<Tuple> {
    Subscription* subscription_;

   public:
    MySubscriber() {
      std::cout << "   Subscriber created!!" << std::endl;
      subscriberInstanceCount++;
    }
    ~MySubscriber() {
      std::cout << "   Subscriber destroyed!!" << std::endl;
      subscriberInstanceCount--;
    }

    void onNext(const Tuple& value) override {
      std::cout << "   Subscriber received value& " << value.a << " on thread "
                << std::this_thread::get_id() << std::endl;
    }

    void onError(std::exception_ptr e) override {}
    void onComplete() override {}
    void onSubscribe(Subscription* s) override {
      subscription_ = std::move(s);
      subscription_->request((u_long)10);
    }
  };

  class Source : public Subscription {
    std::unique_ptr<Subscriber<Tuple>> subscriber_;
    std::atomic_bool isCancelled{false};

   public:
    Source(std::unique_ptr<Subscriber<Tuple>> subscriber)
        : subscriber_(std::move(subscriber)) {
      std::cout << "   Subscription created!!" << std::endl;
      subscriptionInstanceCount++;
    }
    ~Source() {
      std::cout << "   Subscription destroyed!!" << std::endl;
      subscriptionInstanceCount--;
    }

    void start() {
      subscriber_->onSubscribe(this);
    }

    void cancel() override {
      isCancelled = true;
    }
    void request(int64_t n) override {
      // ignoring n for tests ... DO NOT DO THIS FOR REAL
      subscriber_->onNext(Tuple{1, 2});
      subscriber_->onNext(Tuple{2, 3});
      subscriber_->onNext(Tuple{3, 4});
      subscriber_->onComplete();
    }
  };

  {
    static std::atomic_int counter{2};
    // create Flowable around Source
    auto a = Flowable<Tuple>::create(
        [](std::unique_ptr<Subscriber<Tuple>> subscriber) {
          std::cout << "Flowable subscribed to ... starting new thread ..."
                    << std::endl;
          try {
            std::thread([subscriber_ = std::move(subscriber)]() mutable {
              try {
                std::cout << "*** Running in another thread!!!!!" << std::endl;
                // force artificial delay (no race condition, just want to
                // trigger scheduling/async)
                /* sleep override */
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // now do work
                Source s(std::move(subscriber_));
                s.start();
                // assuming pipeline is synchronous, which it is in this test
                counter--;
                std::cout << "Done thread" << std::endl;
              } catch (std::exception& e) {
                std::cout << e.what() << std::endl;
              }
            }).detach();

          } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
          }
        });

    EXPECT_EQ(0, subscriptionInstanceCount);

    std::cout << "Thread counter: " << counter << std::endl;
    std::cout << "About to subscribe (1) ..." << std::endl;

    { a->subscribe(std::make_unique<MySubscriber>()); }
    while (counter > 1) {
      std::cout << "waiting for completion" << std::endl;
      // TODO use condition variable in MySubscriber
      /* sleep override */
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // ensure Subscriber and Subscription are destroyed
    EXPECT_EQ(0, subscriptionInstanceCount);

    std::cout << "About to subscribe (2) ..." << std::endl;

    { a->subscribe(std::make_unique<MySubscriber>()); }
    while (counter > 0) {
      std::cout << "waiting for completion" << std::endl;
      // TODO use condition variable in MySubscriber
      /* sleep override */
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // ensure Subscriber and Subscription are destroyed
    EXPECT_EQ(0, subscriptionInstanceCount);

    std::cout << "Finished ... remaining instances == " << Tuple::instanceCount
              << std::endl;
  }
  std::cout << "... after block" << std::endl;
  EXPECT_EQ(0, Tuple::instanceCount);
  EXPECT_EQ(0, subscriptionInstanceCount);
  EXPECT_EQ(0, subscriberInstanceCount);
  std::cout << "-----------------------------" << std::endl;
}

static std::shared_ptr<reactivestreams_yarpl::Publisher<long>>
getDataAsPublisher() {
  return Flowables::range(1, 4);
}

TEST(FlowableCreateSubscribe, TestReturnTypePublisher) {
  auto ts = TestSubscriber<long>::create();
  getDataAsPublisher()->subscribe(ts->unique_subscriber());
}

TEST(FlowableCreateSubscribe, TestReturnTypePublisherToFlowable) {
  auto ts = TestSubscriber<long>::create();
  Flowables::fromPublisher(getDataAsPublisher())
      ->take(3)
      ->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(3);
}

static std::shared_ptr<Flowable<long>> getDataAsFlowable() {
  return Flowables::range(1, 4);
}

TEST(FlowableCreateSubscribe, TestReturnTypeFlowable) {
  auto ts = TestSubscriber<long>::create();
  getDataAsFlowable()->take(3)->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(3);
}
