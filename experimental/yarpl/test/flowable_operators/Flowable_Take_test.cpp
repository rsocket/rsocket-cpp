// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_TestSubscriber.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

TEST(FlowableTake, Take5) {
  auto ts = TestSubscriber<long>::create();
  Flowables::range(1, 10)->take(5)->subscribe(ts->unique_subscriber());
  ts->awaitTerminalEvent();
  ts->assertValueCount(5);
}
