// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "yarpl/Observable.h"

using namespace yarpl::observable;

//TEST(Subscription, SubscriptionOnCancelInvocation) {
//  bool onCancelInvoked = false;
//  auto s = Subscription::create([&onCancelInvoked]() {
//    std::cout << "onCancel function invoked" << std::endl;
//    onCancelInvoked = true;
//  });
//  EXPECT_FALSE(s->isCanceled());
//  EXPECT_FALSE(onCancelInvoked);
//  s->cancel();
//  // onCancel should have been invoked
//  EXPECT_TRUE(s->isCanceled());
//  EXPECT_TRUE(onCancelInvoked);
//  // reset
//  onCancelInvoked = false;
//  // try canceling again
//  s->cancel();
//  // the onCancel function should not have been invoked again
//  EXPECT_FALSE(onCancelInvoked);
//}
