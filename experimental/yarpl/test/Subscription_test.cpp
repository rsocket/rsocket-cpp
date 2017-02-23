#include <gtest/gtest.h>
#include <yarpl/Observable.h>

using namespace yarpl::observable;

TEST(Subscription, SubscriptionOnCancelInvocation) {
  bool onCancelInvoked = false;
  auto s = Subscription::create([&onCancelInvoked]() {
    std::cout << "onCancel function invoked" << std::endl;
    onCancelInvoked = true;
  });
  EXPECT_EQ(false, s->isCanceled());
  EXPECT_EQ(false, onCancelInvoked);
  s->cancel();
  // onCancel should have been invoked
  EXPECT_EQ(true, s->isCanceled());
  EXPECT_EQ(true, onCancelInvoked);
  // reset
  onCancelInvoked = false;
  // try canceling again
  s->cancel();
  // the onCancel function should not have been invoked again
  EXPECT_EQ(false, onCancelInvoked);
}