// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/flowable/Subscriber.h"
#include "yarpl/test_utils/Mocks.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace yarpl::mocks;
using namespace testing;

namespace {

TEST(FlowableSubscriberTest, TestBasicFunctionality) {
  Sequence subscriber_seq;
  auto subscriber = std::make_shared<StrictMock<MockBaseSubscriber<int>>>();

  EXPECT_CALL(*subscriber, onSubscribeImpl())
    .Times(1)
    .InSequence(subscriber_seq)
    .WillOnce(Invoke([&] {
      subscriber->request(3);
    }));
  EXPECT_CALL(*subscriber, onNextImpl(5))
    .Times(1)
    .InSequence(subscriber_seq);
  EXPECT_CALL(*subscriber, onCompleteImpl())
    .Times(1)
    .InSequence(subscriber_seq);

  auto subscription = std::make_shared<StrictMock<MockSubscription>>();
  EXPECT_CALL(*subscription, request_(3))
    .Times(1)
    .WillOnce(InvokeWithoutArgs([&] {
      subscriber->onNext(5);
      subscriber->onComplete();
    }));

  subscriber->onSubscribe(subscription);
}

TEST(FlowableSubscriberTest, TestKeepRefToThisIsDisabled) {
  auto subscriber = std::make_shared<StrictMock<MockBaseSubscriber<int, false>>>();
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();

  // tests that only a single reference exists to the Subscriber; clearing
  // reference in `auto subscriber` would cause it to deallocate
  {
    InSequence s;
    EXPECT_CALL(*subscriber, onSubscribeImpl()).Times(1).WillOnce(Invoke([&] {
      EXPECT_EQ(1L, subscriber.use_count());
    }));
  }

  subscriber->onSubscribe(subscription);
}
TEST(FlowableSubscriberTest, TestKeepRefToThisIsEnabled) {
  auto subscriber = std::make_shared<StrictMock<MockBaseSubscriber<int>>>();
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();

  // tests that only a reference is held somewhere on the stack, so clearing
  // references to `BaseSubscriber` while in a signaling method won't
  // deallocate it (until it's safe to do so)
  {
    InSequence s;
    EXPECT_CALL(*subscriber, onSubscribeImpl()).Times(1).WillOnce(Invoke([&] {
      EXPECT_EQ(2L, subscriber.use_count());
    }));
  }

  subscriber->onSubscribe(subscription);
}

}
