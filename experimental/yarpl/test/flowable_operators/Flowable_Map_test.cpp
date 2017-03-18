// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_TestSubscriber.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

TEST(FlowableMap, ToString) {
  auto ts = TestSubscriber<std::string>::create();
  Flowables::range(1, 10)
      ->map([](auto i) { return "asString->" + std::to_string(i); })
      ->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(10);
  ASSERT_EQ("asString->1", ts->getValueAt(0));
  ASSERT_EQ("asString->10", ts->getValueAt(9));
}
