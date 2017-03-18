// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <atomic>
#include <sstream>
#include <string>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_TestSubscriber.h"
#include "yarpl/ThreadScheduler.h"

using namespace yarpl::flowable;

TEST(FlowableSubscribeOn, SubscribeToRangeOnThread) {
  std::stringstream mt;
  mt << std::this_thread::get_id();
  std::string mainThreadId = mt.str();
  std::cout << "Main thread id: " << mainThreadId << std::endl;

  auto ts = TestSubscriber<std::string>::create(2);
  yarpl::ThreadScheduler threadScheduler;
  Flowables::range(1, 5)
      ->subscribeOn(threadScheduler)
      ->map([](auto i) {
        std::stringstream s;
        s << std::this_thread::get_id();
        std::cout << " value " << i << " on thread: " << s.str() << std::endl;
        return s.str();
      })
      ->subscribe(ts->unique_subscriber());

  for (;;) {
    if (ts->getValueCount() == 2) {
      std::cout << "got 2 values" << std::endl;
      // request the rest
      ts->requestMore(10);
      break;
    }
  }

  ts->awaitTerminalEvent();
  ts->assertValueCount(5);
  ASSERT_NE(mainThreadId, ts->getValueAt(0));
  ASSERT_NE(mainThreadId, ts->getValueAt(1));
  ASSERT_NE(mainThreadId, ts->getValueAt(2));
  ASSERT_NE(mainThreadId, ts->getValueAt(3));
  ASSERT_NE(mainThreadId, ts->getValueAt(4));
}
