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

#include "yarpl/flowable/PublishProcessor.h"
#include <gtest/gtest.h>
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"

using namespace yarpl;
using namespace yarpl::flowable;

TEST(PublishProcessorTest, OnNextTest) {
  auto pp = PublishProcessor<int>::create();

  auto subscriber = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber);

  pp->onNext(1);
  pp->onNext(2);
  pp->onNext(3);

  EXPECT_EQ(subscriber->values(), std::vector<int>({1, 2, 3}));

  // cancel the subscription as its a cyclic reference
  subscriber->cancel();
}

TEST(PublishProcessorTest, OnCompleteTest) {
  auto pp = PublishProcessor<int>::create();

  auto subscriber = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber);

  pp->onNext(1);
  pp->onNext(2);
  pp->onComplete();

  EXPECT_EQ(
      subscriber->values(),
      std::vector<int>({
          1,
          2,
      }));
  EXPECT_TRUE(subscriber->isComplete());

  auto subscriber2 = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber2);
  EXPECT_EQ(subscriber2->values(), std::vector<int>());
  EXPECT_TRUE(subscriber2->isComplete());
}

TEST(PublishProcessorTest, OnErrorTest) {
  auto pp = PublishProcessor<int>::create();

  auto subscriber = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber);

  pp->onNext(1);
  pp->onNext(2);
  pp->onError(std::runtime_error("error!"));

  EXPECT_EQ(
      subscriber->values(),
      std::vector<int>({
          1,
          2,
      }));
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), "error!");

  auto subscriber2 = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber2);
  EXPECT_EQ(subscriber2->values(), std::vector<int>());
  EXPECT_TRUE(subscriber2->isError());
}

TEST(PublishProcessorTest, OnNextMultipleSubscribersTest) {
  auto pp = PublishProcessor<int>::create();

  auto subscriber1 = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber1);
  auto subscriber2 = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber2);

  pp->onNext(1);
  pp->onNext(2);
  pp->onNext(3);

  EXPECT_EQ(subscriber1->values(), std::vector<int>({1, 2, 3}));
  EXPECT_EQ(subscriber2->values(), std::vector<int>({1, 2, 3}));

  subscriber1->cancel();
  subscriber2->cancel();
}

TEST(PublishProcessorTest, OnNextSlowSubscriberTest) {
  auto pp = PublishProcessor<int>::create();

  auto subscriber1 = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber1);
  auto subscriber2 = std::make_shared<TestSubscriber<int>>(1);
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber2);

  pp->onNext(1);
  pp->onNext(2);
  pp->onNext(3);

  EXPECT_EQ(subscriber1->values(), std::vector<int>({1, 2, 3}));
  subscriber1->cancel();

  EXPECT_EQ(subscriber2->values(), std::vector<int>({1}));
  EXPECT_TRUE(subscriber2->isError());
  EXPECT_EQ(
      subscriber2->exceptionWrapper().type(),
      typeid(MissingBackpressureException));
}

TEST(PublishProcessorTest, CancelTest) {
  auto pp = PublishProcessor<int>::create();

  auto subscriber = std::make_shared<TestSubscriber<int>>();
  pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber);

  pp->onNext(1);
  pp->onNext(2);

  subscriber->cancel();

  pp->onNext(3);
  pp->onNext(4);

  EXPECT_EQ(subscriber->values(), std::vector<int>({1, 2}));

  subscriber->onComplete(); // to break any reference cycles
}

TEST(PublishProcessorTest, OnMultipleSubscribersMultithreadedWithErrorTest) {
  auto pp = PublishProcessor<int>::create();

  std::vector<std::thread> threads;
  std::atomic<size_t> threadsDone{0};

  for (int i = 0; i < 100; i++) {
    threads.push_back(std::thread([&] {
      for (int j = 0; j < 100; j++) {
        auto subscriber = std::make_shared<TestSubscriber<int>>(1);
        pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber);

        subscriber->awaitTerminalEvent(std::chrono::milliseconds(500));

        EXPECT_EQ(subscriber->values().size(), 1ULL);

        EXPECT_TRUE(subscriber->isError());
        EXPECT_EQ(
            subscriber->exceptionWrapper().type(),
            typeid(MissingBackpressureException));
      }
      ++threadsDone;
    }));
  }

  int k = 0;
  while (threadsDone < threads.size()) {
    pp->onNext(k++);
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(PublishProcessorTest, OnMultipleSubscribersMultithreadedTest) {
  auto pp = PublishProcessor<int>::create();

  std::vector<std::thread> threads;
  std::atomic<size_t> subscribersReady{0};
  std::atomic<size_t> threadsDone{0};

  for (int i = 0; i < 100; i++) {
    threads.push_back(std::thread([&] {
      auto subscriber = std::make_shared<TestSubscriber<int>>();
      pp->toFlowable(BackpressureStrategy::ERROR)->subscribe(subscriber);

      ++subscribersReady;
      subscriber->awaitTerminalEvent(std::chrono::milliseconds(50));

      EXPECT_EQ(subscriber->values(), std::vector<int>({1, 2, 3, 4, 5}));
      EXPECT_FALSE(subscriber->isError());
      EXPECT_TRUE(subscriber->isComplete());

      ++threadsDone;
    }));
  }

  while (subscribersReady < threads.size())
    ;

  pp->onNext(1);
  pp->onNext(2);
  pp->onNext(3);
  pp->onNext(4);
  pp->onNext(5);
  pp->onComplete();

  for (auto& thread : threads) {
    thread.join();
  }
}
