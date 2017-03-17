
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
 *
 * @param scheduler
 * @param subscriber
 */
void runHandlerFlowable(
    Scheduler& scheduler,
    std::unique_ptr<Subscriber<long>> subscriber) {
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
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      auto s_ =
          new yarpl::flowable::sources::RangeSubscription(1, 100, std::move(s));
      s_->start();
    };
  };
  unsafeCreateUniqueFlowable<long>(Handler())
      .subscribeOn(scheduler)
      .take(50)
      .subscribe(std::move(subscriber));
}

TEST(FlowableLifecycle, HandlerClass) {
  ThreadScheduler scheduler;
  auto ts = TestSubscriber<long>::create();
  runHandlerFlowable(scheduler, ts->unique_subscriber());
  std::cout << "after runHandlerFlowable" << std::endl;
  ts->awaitTerminalEvent();
  ts->assertValueCount(50);
}