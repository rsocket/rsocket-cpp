// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/ThreadScheduler.h"
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

TEST(FlowableRange, 1_to_100) {
  auto f = Flowables::range(1, 100);
  auto ts = TestSubscriber<long>::create(200);
  f->subscribe(ts->unique_subscriber());
  ts->assertValueCount(100);
}

TEST(FlowableRange, completes) {
  auto f = Flowables::range(1, 100);
  auto ts = TestSubscriber<long>::create(200);
  f->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(100);
}

TEST(FlowableRange, 1_to_100_in_jumps) {
  auto f = Flowables::range(1, 100);
  auto ts = TestSubscriber<long>::create(20);
  f->subscribe(ts->unique_subscriber());
  ts->assertValueCount(20);
  ts->requestMore(40);
  ts->assertValueCount(60);
  ts->requestMore(50);
  ts->assertValueCount(100);
}

/**
 * Interleave requesting more while source is still emitting
 */
TEST(FlowableRange, 1_to_100_in_concurrent_jumps) {
  class MySubscriber : public Subscriber<long> {
   public:
    void onSubscribe(Subscription* subscription) {
      s_ = subscription;
      requested = 10;
      s_->request(10);
    }

    void onNext(const long& t) {
      acceptAndRequestMoreIfNecessary();
      std::cout << "onNext& " << t << std::endl;
    }

    void onNext(long&& t) {
      acceptAndRequestMoreIfNecessary();
      std::cout << "onNext&& " << t << std::endl;
    }

    void onComplete() {
      std::cout << "onComplete " << std::endl;
    }

    void onError(std::exception_ptr) {
      std::cout << "onError " << std::endl;
    }

   private:
    Subscription* s_;
    int requested{0};

    void acceptAndRequestMoreIfNecessary() {
      if (--requested == 2) {
        std::cout << "Request more..." << std::endl;
        requested += 8;
        s_->request(8);
      }
    }
  };

  auto f = Flowables::range(1, 100);
  auto ts = TestSubscriber<long>::create(std::make_unique<MySubscriber>());
  f->subscribe(ts->unique_subscriber());
  ts->assertValueCount(100);
}

TEST(FlowableRange, 1_to_100_cancel) {
  auto f = Flowables::range(1, 100);
  auto ts = TestSubscriber<long>::create(40);
  f->subscribe(ts->unique_subscriber());
  ts->cancel();
  ts->assertValueCount(40);
}

/**
 * Test that cancellation works correctly across thread boundaries
 * to stop emission.
 */
TEST(FlowableRange, DISABLED_async_cancellation) {
  ThreadScheduler threadScheduler;
  const int MAX_SIZE = 500000000;
  auto f = Flowable<long>::create([MAX_SIZE](auto s) {
    auto r_ = new yarpl::flowable::sources::RangeSubscription(
        1, MAX_SIZE, std::move(s));
    r_->start();
  });
  auto ts = TestSubscriber<long>::create();
  f->subscribeOn(threadScheduler)->subscribe(ts->unique_subscriber());
  std::cout << "after subscribe" << std::endl;
  ts->cancel();
  std::cout << "Received " << ts->getValueCount() << " values" << std::endl;
  // it should cancel well before hitting the MAX_SIZE
  EXPECT_LT(ts->getValueCount(), MAX_SIZE);
}
