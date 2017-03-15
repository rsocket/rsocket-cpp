
// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

void runHandlerFlowable(std::unique_ptr<Subscriber<long>> subscriber) {
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
    unsafeCreateUniqueFlowable<long>(Handler())->subscribe(std::move(subscriber));
}

TEST(FlowableLifecycle, HandlerClass) {
  auto ts = TestSubscriber<long>::create();
  runHandlerFlowable(ts->unique_subscriber());
  std::cout << "after runHandlerFlowable" << std::endl;
  ts->awaitTerminalEvent();
  ts->assertValueCount(100);
}