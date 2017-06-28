// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/Flowable_TestSubscriber.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

TEST(FlowableChaining, Lift) {
  class MySubscriber : public Subscriber<long> {
   public:
    MySubscriber(
        std::unique_ptr<reactivestreams_yarpl::Subscriber<std::string>> s)
        : downstream_(std::move(s)) {}

    void onSubscribe(Subscription* subscription) {
      s_ = subscription;
      s_->request(10000);
    }

    void onNext(const long& t) {
      std::cout << "onNext& inside lift " << t << std::endl;
      downstream_->onNext("hello inside Lift");
    }

    void onNext(long&& t) {
      std::cout << "onNext&& inside lift " << t << std::endl;
      downstream_->onNext("hello");
    }

    void onComplete() {
      std::cout << "onComplete " << std::endl;
    }

    void onError(std::exception_ptr error) {
      std::cout << "onError " << std::endl;
    }

   private:
    Subscription* s_;
    std::unique_ptr<reactivestreams_yarpl::Subscriber<std::string>> downstream_;
  };

  class LongToStringFunctor {
   public:
    std::unique_ptr<reactivestreams_yarpl::Subscriber<long>> operator()(
        std::unique_ptr<reactivestreams_yarpl::Subscriber<std::string>> s) {
      std::cout << "hello" << std::endl;
      return std::make_unique<MySubscriber>(std::move(s));
    }
  };

  auto ts = TestSubscriber<std::string>::create();
  Flowables::range(1, 10)
      ->lift<std::string>(LongToStringFunctor())
      ->subscribe(ts->unique_subscriber());
}

TEST(FlowableChaining, Map) {
  class MySubscriber : public Subscriber<std::string> {
   public:
    void onSubscribe(Subscription* subscription) {
      subscription->request(100);
    }

    void onNext(const std::string& t) {
      std::cout << "onNext& " << t << std::endl;
    }

    void onNext(std::string&& t) {
      std::cout << "onNext&& " << t << std::endl;
    }

    void onComplete() {
      std::cout << "onComplete " << std::endl;
    }

    void onError(std::exception_ptr error) {
      std::cout << "onError " << std::endl;
    }
  };

  auto ts =
      TestSubscriber<std::string>::create(std::make_unique<MySubscriber>());
  Flowables::range(0, 20)
      ->map([](auto v) { return "hello via map " + std::to_string(v); })
      ->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(20);
}

TEST(FlowableChaining, rangeMapTake) {
  auto a = Flowables::range(1, 100);
  auto b = a->map([](auto i) { return "hello->" + std::to_string(i); });
  auto c = b->take(10);

  c->subscribe(Subscribers::create<std::string>(
      [](auto t) { std::cout << "Value received: " << t << std::endl; }));
}

TEST(FlowableChaining, rangeMapTakeBranched) {
  auto a = Flowables::range(1, 100);
  auto b = a->take(10);
  auto c = b->map([](auto i) { return "hello->" + std::to_string(i); });

  c->subscribe(Subscribers::create<std::string>(
      [](auto t) { std::cout << "Value received: " << t << std::endl; }));

  // this should not work, but it does
  auto c2 = b->map([](auto i) { return "should break->" + std::to_string(i); });
  c2->subscribe(Subscribers::create<std::string>(
      [](auto t) { std::cout << "Value received2: " << t << std::endl; }));
}

TEST(FlowableChaining, customSourceThenMapTakeBranched) {
  class InfiniteIntegersSource : public Subscription {
    std::unique_ptr<Subscriber<int>> subscriber_;
    std::atomic_bool isCancelled{false};

    InfiniteIntegersSource(InfiniteIntegersSource&&) = default;
    InfiniteIntegersSource(const InfiniteIntegersSource&) = delete;
    InfiniteIntegersSource& operator=(InfiniteIntegersSource&&) = default;
    InfiniteIntegersSource& operator=(const InfiniteIntegersSource&) = delete;

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

  class OnSubscribeFunctor {
   public:
    OnSubscribeFunctor() = default;
    ~OnSubscribeFunctor() {
      std::cout << "******* DESTROY OnSubscribeFunctor" << std::endl;
    }

    OnSubscribeFunctor(OnSubscribeFunctor&&) {
      std::cout << "******* moving OnSubscribeFunctor" << std::endl;
      // no members to move
    }

    OnSubscribeFunctor(const OnSubscribeFunctor&) = delete;

    OnSubscribeFunctor& operator=(OnSubscribeFunctor&& other) {
      std::cout << "******* moving() OnSubscribeFunctor" << std::endl;
      // no members to move
      return *this;
    }

    OnSubscribeFunctor& operator=(const OnSubscribeFunctor&) = delete;

    void operator()(
        std::unique_ptr<reactivestreams_yarpl::Subscriber<int>> subscriber) {
      std::cout << ">>> subscribing to InfiniteIntegersSource" << std::endl;
      InfiniteIntegersSource::subscribe(std::move(subscriber));
    }
  };

  // create Flowable around InfiniteIntegersSource
  auto a = Flowable<int>::create(OnSubscribeFunctor());

  {
    auto b = a->take(10);
    auto c = b->map([](auto i) { return "hello->" + std::to_string(i); });

    c->subscribe(Subscribers::create<std::string>(
        [](auto t) { std::cout << "Value received: " << t << std::endl; }));
  }

  {
    auto b = a->take(10);
    auto c = b->map([](auto i) { return "hello again->" + std::to_string(i); });

    c->subscribe(Subscribers::create<std::string>([](auto t) {
      std::cout << "Value received again: " << t << std::endl;
    }));
  }
}
