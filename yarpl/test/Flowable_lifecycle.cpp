// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/ThreadScheduler.h"
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

using namespace yarpl::flowable;
using namespace yarpl;
using namespace reactivestreams_yarpl;

/**
 * Test how things behave when the Flowable goes out of scope.
 * The Handler class is *just* the onSubscribe func. The real lifetime
 * is in the Subscription which the Handler kicks off via a 'new' to
 * put on the heap.
 */
static auto runHandlerFlowable(Scheduler& scheduler) {
  class Handler {
   public:
    Handler() = default;
    ~Handler() {
      std::cout << "DESTROY Handler" << std::endl;
    }
    Handler(Handler&&) = default; // only allow std::move
    Handler(const Handler&) = delete;
    Handler& operator=(Handler&&) = default; // only allow std::move
    Handler& operator=(const Handler&) = delete;

    void operator()(std::unique_ptr<Subscriber<long>> s) {
      // want to test that lifecycle works when thread scheduling occurs
      /* sleep override */
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      auto s_ =
          new yarpl::flowable::sources::RangeSubscription(1, 100, std::move(s));
      s_->start();
    }
  };
  return Flowable<long>::create(Handler())->subscribeOn(scheduler)->take(50);
}

TEST(FlowableLifecycle, DISABLED_HandlerClass) {
  ThreadScheduler scheduler;

  /**
   * Make sure we interleave request(n) with emission
   * so it forces scheduling and concurrency.
   */
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

  auto ts = TestSubscriber<long>::create(std::make_unique<MySubscriber>());
  auto f = runHandlerFlowable(scheduler);
  std::cout << "received f, now subscribe" << std::endl;
  f->subscribe(ts->unique_subscriber());
  std::cout << "after runHandlerFlowable" << std::endl;
  ts->awaitTerminalEvent();
  ts->assertValueCount(50);
}
