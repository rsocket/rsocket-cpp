
// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

// TODO fix this ... non-deterministic how the thread behaves ... need a proper
// subscribeOn(Scheduler)

void runHandlerFlowable(std::unique_ptr<Subscriber<long>> subscriber) {
  class Handler {
   public:
    Handler() = default;
    Handler(Handler&&) = default; // only allow std::move
    Handler(const Handler&) = delete;
    Handler& operator=(Handler&&) = default; // only allow std::move
    Handler& operator=(const Handler&) = delete;

    void operator()(std::unique_ptr<Subscriber<long>> s) {
      std::thread([s = std::move(s)]() mutable {
        std::cout << "in thread" << std::endl;
        auto s_ = new yarpl::flowable::sources::RangeSubscription(
            1, 100, std::move(s));
        s_->start();
        std::cout << "end of thread block" << std::endl;
      }).detach();
      std::cout << "after launch of thread" << std::endl;
    };
  };
  unsafeCreateUniqueFlowable<long>(Handler())->subscribe(std::move(subscriber));
}

TEST(FlowableLifecycle, HandlerClass) {
  auto ts = TestSubscriber<long>::create();
  runHandlerFlowable(ts->unique_subscriber());
  std::cout << "after runHandlerFlowable" << std::endl;
  ts->awaitTerminalEvent();
  ts->assertValueCount(100);
}

void runHandlerFlowable2(std::unique_ptr<Subscriber<long>> subscriber) {
  class Handler {
   public:
    void operator()(std::unique_ptr<Subscriber<long>> s) {
      std::thread([s = std::move(s)]() mutable {
        std::cout << "in thread" << std::endl;
        auto s_ = new yarpl::flowable::sources::RangeSubscription(
            1, 100, std::move(s));
        s_->start();
        std::cout << "end of thread block" << std::endl;
      }).detach();
      std::cout << "after launch of thread" << std::endl;
    };
  };
  unsafeCreateUniqueFlowable<long>(Handler())->take(10)->subscribe(
      std::move(subscriber));
}

TEST(FlowableLifecycle, HandlerClassWithOperatorChain) {
  auto ts = TestSubscriber<long>::create();
  runHandlerFlowable2(ts->unique_subscriber());
  std::cout << "after runHandlerFlowable2" << std::endl;
  ts->awaitTerminalEvent();
  ts->assertValueCount(10);
}