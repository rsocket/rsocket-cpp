// Copyright (c) Facebook, Inc. and its affiliates.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "yarpl/flowable/Subscriber.h"
#include "yarpl/test_utils/Mocks.h"

using namespace yarpl;
using namespace yarpl::flowable;
using namespace yarpl::mocks;
using namespace testing;

namespace {

TEST(FlowableSubscriberTest, CreateSubscriber) {
  int calls{0};
  struct Functor {
    explicit Functor(int& calls) : calls_(calls) {}
    // If we update the template definition of the Subscriber,
    // then we should comment out this method and observe the compiler output
    // with and without the change.
    void operator()(int) & {
      ++calls_;
    }
    void operator()(int) && {
      FAIL() << "onNext lambda should be stored as l-value";
    }
    void operator()(std::string) const& {
      ++calls_;
    }
    void operator()(std::string) const&& {
      FAIL() << "onNext lambda should be stored as l-value";
    }
    int& calls_;
  };
  auto s1 = Subscriber<int>::create(Functor(calls));
  s1->onSubscribe(yarpl::flowable::Subscription::create());
  s1->onNext(1);
  EXPECT_EQ(1, calls);

  auto s2 = Subscriber<long>::create(Functor(calls));
  s2->onSubscribe(yarpl::flowable::Subscription::create());
  s2->onNext((long)1);
  EXPECT_EQ(2, calls);

  auto s3 = Subscriber<std::string>::create(Functor(calls));
  s3->onSubscribe(yarpl::flowable::Subscription::create());
  s3->onNext("test");
  EXPECT_EQ(3, calls);

  // by reference
  auto f = Functor(calls);
  auto s4 = Subscriber<int>::create(f);
  s4->onSubscribe(yarpl::flowable::Subscription::create());
  s4->onNext(1);
  EXPECT_EQ(4, calls);
}

TEST(FlowableSubscriberTest, TestBasicFunctionality) {
  Sequence subscriber_seq;
  auto subscriber = std::make_shared<StrictMock<MockBaseSubscriber<int>>>();

  EXPECT_CALL(*subscriber, onSubscribeImpl())
      .Times(1)
      .InSequence(subscriber_seq)
      .WillOnce(Invoke([&] { subscriber->request(3); }));
  EXPECT_CALL(*subscriber, onNextImpl(5)).Times(1).InSequence(subscriber_seq);
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
  auto subscriber =
      std::make_shared<StrictMock<MockBaseSubscriber<int, false>>>();
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();

  // tests that only a single reference exists to the Subscriber; clearing
  // reference in `auto subscriber` would cause it to deallocate
  {
    InSequence s;
    EXPECT_CALL(*subscriber, onSubscribeImpl()).Times(1).WillOnce(Invoke([&] {
      EXPECT_EQ(1UL, subscriber.use_count());
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
      EXPECT_EQ(2UL, subscriber.use_count());
    }));
  }

  subscriber->onSubscribe(subscription);
}

TEST(FlowableSubscriberTest, AutoFlowControl) {
  size_t count = 0;
  auto subscriber = Subscriber<int>::create(
      [&](int value) {
        ++count;
        EXPECT_EQ(value, count);
      },
      1);
  auto subscription = std::make_shared<StrictMock<MockSubscription>>();

  EXPECT_CALL(*subscription, request_(1))
      .Times(3)
      .WillOnce(InvokeWithoutArgs([&] { subscriber->onNext(1); }))
      .WillOnce(InvokeWithoutArgs([&] {
        subscriber->onNext(2);
        subscriber->onComplete();
      }));

  subscriber->onSubscribe(subscription);
}
} // namespace
