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

#include <folly/io/async/ScopedEventBaseThread.h>
#include <folly/synchronization/Baton.h>
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include <vector>
#include "yarpl/Flowable.h"
#include "yarpl/Observable.h"
#include "yarpl/flowable/TestSubscriber.h"
#include "yarpl/observable/TestObserver.h"

using namespace yarpl::flowable;
using namespace yarpl::observable;

constexpr std::chrono::milliseconds timeout{100};

TEST(FlowableTests, SubscribeOnWorksAsExpected) {
  folly::ScopedEventBaseThread worker;

  auto f = Flowable<std::string>::create([&](auto& subscriber, auto req) {
    EXPECT_TRUE(worker.getEventBase()->isInEventBaseThread());
    EXPECT_EQ(1, req);
    subscriber.onNext("foo");
    subscriber.onComplete();
  });

  auto subscriber = std::make_shared<TestSubscriber<std::string>>(1);
  f->subscribeOn(*worker.getEventBase())->subscribe(subscriber);
  subscriber->awaitTerminalEvent(std::chrono::milliseconds(100));
  EXPECT_EQ(1, subscriber->getValueCount());
  EXPECT_TRUE(subscriber->isComplete());
}

TEST(ObservableTests, SubscribeOnWorksAsExpected) {
  folly::ScopedEventBaseThread worker;

  auto f = Observable<std::string>::create([&](auto observer) {
    EXPECT_TRUE(worker.getEventBase()->isInEventBaseThread());
    observer->onNext("foo");
    observer->onComplete();
  });

  auto observer = std::make_shared<TestObserver<std::string>>();
  f->subscribeOn(*worker.getEventBase())->subscribe(observer);
  observer->awaitTerminalEvent(std::chrono::milliseconds(100));
  EXPECT_EQ(1, observer->getValueCount());
  EXPECT_TRUE(observer->isComplete());
}

TEST(FlowableTests, ObserveOnWorksAsExpectedSuccess) {
  folly::ScopedEventBaseThread worker;
  folly::Baton<> subscriber_complete;

  auto f = Flowable<std::string>::create([&](auto& subscriber, auto req) {
    EXPECT_EQ(1, req);
    subscriber.onNext("foo");
    subscriber.onComplete();
  });

  bool calledOnNext{false};

  f->observeOn(*worker.getEventBase())
      ->subscribe(
          // onNext
          [&](std::string s) {
            EXPECT_TRUE(worker.getEventBase()->isInEventBaseThread());
            EXPECT_EQ(s, "foo");
            calledOnNext = true;
          },

          // onError
          [&](folly::exception_wrapper) { FAIL(); },

          // onComplete
          [&] {
            EXPECT_TRUE(worker.getEventBase()->isInEventBaseThread());
            EXPECT_TRUE(calledOnNext);
            subscriber_complete.post();
          },

          1 /* initial request(n) */
      );

  subscriber_complete.timed_wait(timeout);
}

TEST(FlowableTests, ObserveOnWorksAsExpectedError) {
  folly::ScopedEventBaseThread worker;
  folly::Baton<> subscriber_complete;

  auto f = Flowable<std::string>::create([&](auto& subscriber, auto req) {
    EXPECT_EQ(1, req);
    subscriber.onError(std::runtime_error("oops!"));
  });

  f->observeOn(*worker.getEventBase())
      ->subscribe(
          // onNext
          [&](std::string s) { FAIL(); },

          // onError
          [&](folly::exception_wrapper) {
            EXPECT_TRUE(worker.getEventBase()->isInEventBaseThread());
            subscriber_complete.post();
          },

          // onComplete
          [&] { FAIL(); },

          1 /* initial request(n) */
      );

  subscriber_complete.timed_wait(timeout);
}

TEST(FlowableTests, BothObserveAndSubscribeOn) {
  folly::ScopedEventBaseThread subscriber_eb;
  folly::ScopedEventBaseThread producer_eb;
  folly::Baton<> subscriber_complete;

  auto f = Flowable<std::string>::create([&](auto& subscriber, auto req) {
             EXPECT_EQ(1, req);
             EXPECT_TRUE(producer_eb.getEventBase()->isInEventBaseThread());
             subscriber.onNext("foo");
             subscriber.onComplete();
           })
               ->subscribeOn(*producer_eb.getEventBase())
               ->observeOn(*subscriber_eb.getEventBase());

  bool calledOnNext{false};

  f->subscribe(
      // onNext
      [&](std::string s) {
        EXPECT_TRUE(subscriber_eb.getEventBase()->isInEventBaseThread());
        EXPECT_EQ(s, "foo");
        calledOnNext = true;
      },

      // onError
      [&](folly::exception_wrapper) { FAIL(); },

      // onComplete
      [&] {
        EXPECT_TRUE(subscriber_eb.getEventBase()->isInEventBaseThread());
        EXPECT_TRUE(calledOnNext);
        subscriber_complete.post();
      },

      1 /* initial request(n) */
  );

  subscriber_complete.timed_wait(timeout);
}

namespace {
class EarlyCancelSubscriber : public yarpl::flowable::BaseSubscriber<int64_t> {
 public:
  EarlyCancelSubscriber(
      folly::EventBase& on_base,
      folly::Baton<>& subscriber_complete)
      : on_base_(on_base), subscriber_complete_(subscriber_complete) {}

  void onSubscribeImpl() override {
    this->request(5);
  }

  void onNextImpl(int64_t n) override {
    if (did_cancel_) {
      FAIL();
    }

    EXPECT_TRUE(on_base_.isInEventBaseThread());
    EXPECT_EQ(n, 1);
    this->cancel();
    did_cancel_ = true;
    subscriber_complete_.post();
  }

  void onErrorImpl(folly::exception_wrapper /*e*/) override {
    FAIL();
  }

  void onCompleteImpl() override {
    FAIL();
  }

  bool did_cancel_{false};
  folly::EventBase& on_base_;
  folly::Baton<>& subscriber_complete_;
};
} // namespace

TEST(FlowableTests, EarlyCancelObserveOn) {
  folly::ScopedEventBaseThread worker;

  folly::Baton<> subscriber_complete;

  Flowable<>::range(1, 100)
      ->observeOn(*worker.getEventBase())
      ->subscribe(std::make_shared<EarlyCancelSubscriber>(
          *worker.getEventBase(), subscriber_complete));

  subscriber_complete.timed_wait(timeout);
}
