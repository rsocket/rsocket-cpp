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

#include <folly/synchronization/Baton.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <condition_variable>

#include "yarpl/Observable.h"
#include "yarpl/flowable/Flowable.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/test_utils/Mocks.h"
#include "yarpl/test_utils/Tuple.h"

// TODO can we eliminate need to import both of these?
using namespace yarpl;
using namespace yarpl::mocks;
using namespace yarpl::observable;
using namespace testing;

namespace {

void unreachable() {
  EXPECT_TRUE(false) << "unreachable code";
}

template <typename T>
class CollectingObserver : public Observer<T> {
 public:
  void onNext(T next) override {
    values_.push_back(std::move(next));
  }

  void onComplete() override {
    Observer<T>::onComplete();
    complete_ = true;
    terminated_ = true;
  }

  void onError(folly::exception_wrapper ex) override {
    Observer<T>::onError(ex);
    error_ = true;
    errorMsg_ = ex.get_exception()->what();
    terminated_ = true;
  }

  std::vector<T>& values() {
    return values_;
  }

  bool complete() const {
    return complete_;
  }

  bool error() const {
    return error_;
  }

  const std::string& errorMsg() const {
    return errorMsg_;
  }

  /**
   * Block the current thread until either onSuccess or onError is called.
   */
  void awaitTerminalEvent(
      std::chrono::milliseconds ms = std::chrono::seconds{1}) {
    // now block this thread
    std::unique_lock<std::mutex> lk(m_);
    // if shutdown gets implemented this would then be released by it
    if (!terminalEventCV_.wait_for(lk, ms, [this] { return terminated_; })) {
      throw std::runtime_error("timeout in awaitTerminalEvent");
    }
  }

 private:
  std::vector<T> values_;
  std::string errorMsg_;
  bool complete_{false};
  bool error_{false};

  bool terminated_{false};
  std::mutex m_;
  std::condition_variable terminalEventCV_;
};

/// Construct a pipeline with a collecting observer against the supplied
/// observable.  Return the items that were sent to the observer.  If some
/// exception was sent, the exception is thrown.
template <typename T>
std::vector<T> run(std::shared_ptr<Observable<T>> observable) {
  auto collector = std::make_shared<CollectingObserver<T>>();
  observable->subscribe(collector);
  collector->awaitTerminalEvent(std::chrono::seconds(1));
  return std::move(collector->values());
}

} // namespace

TEST(Observable, SingleOnNext) {
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    obs->onNext(1);
    obs->onComplete();
  });

  std::vector<int> v;
  a->subscribe(
      Observer<int>::create([&v](const int& value) { v.push_back(value); }));
  EXPECT_EQ(v.at(0), 1);
}

TEST(Observable, MultiOnNext) {
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    obs->onNext(1);
    obs->onNext(2);
    obs->onNext(3);
    obs->onComplete();
  });

  std::vector<int> v;
  a->subscribe(
      Observer<int>::create([&v](const int& value) { v.push_back(value); }));

  EXPECT_EQ(v.at(0), 1);
  EXPECT_EQ(v.at(1), 2);
  EXPECT_EQ(v.at(2), 3);
}

TEST(Observable, OnError) {
  std::string errorMessage("DEFAULT->No Error Message");
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    obs->onError(std::runtime_error("something broke!"));
  });

  a->subscribe(Observer<int>::create(
      [](int) { /* do nothing */ },
      [&errorMessage](folly::exception_wrapper ex) {
        errorMessage = ex.get_exception()->what();
      }));

  EXPECT_EQ("something broke!", errorMessage);
}

/**
 * Assert that all items passed through the Observable get destroyed
 */
TEST(Observable, ItemsCollectedSynchronously) {
  auto a = Observable<Tuple>::create([](std::shared_ptr<Observer<Tuple>> obs) {
    obs->onNext(Tuple{1, 2});
    obs->onNext(Tuple{2, 3});
    obs->onNext(Tuple{3, 4});
    obs->onComplete();
  });

  a->subscribe(Observer<Tuple>::create([](const Tuple& value) {
    std::cout << "received value " << value.a << std::endl;
  }));
}

/*
 * Assert that all items passed through the Observable get
 * copied and destroyed correctly over async boundaries.
 *
 * This is simulating "async" by having an Observer store the items
 * in a Vector which could then be consumed on another thread.
 */
TEST(DISABLED_Observable, ItemsCollectedAsynchronously) {
  auto a = Observable<Tuple>::create([](std::shared_ptr<Observer<Tuple>> obs) {
    std::cout << "-----------------------------" << std::endl;
    obs->onNext(Tuple{1, 2});
    std::cout << "-----------------------------" << std::endl;
    obs->onNext(Tuple{2, 3});
    std::cout << "-----------------------------" << std::endl;
    obs->onNext(Tuple{3, 4});
    std::cout << "-----------------------------" << std::endl;
    obs->onComplete();
  });

  std::vector<Tuple> v;
  v.reserve(10); // otherwise it resizes and copies on each push_back
  a->subscribe(Observer<Tuple>::create([&v](const Tuple& value) {
    std::cout << "received value " << value.a << std::endl;
    // copy into vector
    v.push_back(value);
    std::cout << "done pushing into vector" << std::endl;
  }));

  // 3 copy & 3 move and 3 more copy constructed
  EXPECT_EQ(9, Tuple::createdCount);
  // 3 still exists in the vector, 6 destroyed
  EXPECT_EQ(6, Tuple::destroyedCount);

  std::cout << "Leaving block now so Vector should release Tuples..."
            << std::endl;
}

class TakeObserver : public Observer<int> {
 private:
  const int limit;
  int count = 0;
  std::shared_ptr<yarpl::observable::Subscription> subscription_;
  std::vector<int>& v;

 public:
  TakeObserver(int _limit, std::vector<int>& _v) : limit(_limit), v(_v) {
    v.reserve(5);
  }

  void onSubscribe(
      std::shared_ptr<yarpl::observable::Subscription> s) override {
    subscription_ = std::move(s);
  }

  void onNext(int value) override {
    v.push_back(value);
    if (++count >= limit) {
      //      std::cout << "Cancelling subscription after receiving " << count
      //                << " items." << std::endl;
      subscription_->cancel();
    }
  }

  void onError(folly::exception_wrapper) override {}
  void onComplete() override {}
};

// assert behavior of onComplete after subscription.cancel
TEST(Observable, SubscriptionCancellation) {
  std::atomic_int emitted{0};
  auto a = Observable<int>::create([&](std::shared_ptr<Observer<int>> obs) {
    int i = 0;
    while (!obs->isUnsubscribed() && i <= 10) {
      emitted++;
      obs->onNext(i++);
    }
    if (!obs->isUnsubscribed()) {
      // should be ignored
      obs->onComplete();
    }
  });

  std::vector<int> v;
  a->subscribe(std::make_shared<TakeObserver>(2, v));
  EXPECT_EQ((unsigned long)2, v.size());
  EXPECT_EQ(2, emitted);
}

TEST(Observable, CancelFromDifferentThread) {
  std::atomic_int emitted{0};
  std::mutex m;
  std::condition_variable cv;

  std::atomic<bool> cancelled1{false};
  std::atomic<bool> cancelled2{false};

  std::thread t;
  auto a = Observable<int>::create([&](std::shared_ptr<Observer<int>> obs) {
    t = std::thread([obs, &emitted, &cancelled1]() {
      obs->addSubscription([&]() { cancelled1 = true; });
      while (!obs->isUnsubscribed()) {
        ++emitted;
        obs->onNext(0);
      }
    });
    obs->addSubscription([&]() { cancelled2 = true; });
  });

  auto subscription = a->subscribe([](int) {});

  std::unique_lock<std::mutex> lk(m);
  CHECK(cv.wait_for(
      lk, std::chrono::seconds(1), [&] { return emitted >= 1000; }));

  subscription->cancel();
  t.join();
  CHECK(cancelled1);
  CHECK(cancelled2);
  LOG(INFO) << "cancelled after " << emitted << " items";
}

TEST(Observable, toFlowableDrop) {
  auto a = Observable<>::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::DROP);

  std::vector<int64_t> v;

  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);

  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));
}

TEST(Observable, toFlowableDropWithCancel) {
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    int i = 0;
    while (!obs->isUnsubscribed()) {
      obs->onNext(++i);
    }
  });

  auto f = a->toFlowable(BackpressureStrategy::DROP);

  std::vector<int> v;
  f->take(5)->subscribe(yarpl::flowable::Subscriber<int>::create(
      [&v](const int& value) { v.push_back(value); }));

  EXPECT_EQ(v, std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(Observable, toFlowableErrorStrategy) {
  auto a = Observable<int64_t>::createEx([](auto observer, auto subscription) {
    int64_t i = 1;
    for (; !subscription->isCancelled() && i <= 10; ++i) {
      observer->onNext(i);
    }
    EXPECT_EQ(7, i);
  });
  auto f = a->toFlowable(BackpressureStrategy::ERROR);

  std::vector<int64_t> v;

  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onError_(_))
      .WillOnce(Invoke([&](folly::exception_wrapper ex) {
        EXPECT_TRUE(ex.is_compatible_with<
                    yarpl::flowable::MissingBackpressureException>());
      }));

  f->subscribe(subscriber);

  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));
}

TEST(Observable, toFlowableBufferStrategy) {
  auto a = Observable<>::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::BUFFER);

  std::vector<int64_t> v;

  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));

  subscriber->subscription()->request(5);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
}

TEST(Observable, toFlowableBufferStrategyLimit) {
  std::shared_ptr<Observer<int64_t>> observer;
  std::shared_ptr<Subscription> subscription;

  auto a = Observable<int64_t>::createEx([&](auto o, auto s) {
    observer = std::move(o);
    subscription = std::move(s);
  });
  auto f =
      a->toFlowable(std::make_shared<BufferBackpressureStrategy<int64_t>>(3));

  std::vector<int64_t> v;

  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));

  EXPECT_FALSE(observer);
  EXPECT_FALSE(subscription);

  f->subscribe(subscriber);

  EXPECT_TRUE(observer);
  EXPECT_TRUE(subscription);

  for (size_t i = 1; i <= 5; ++i) {
    observer->onNext(i);
  }

  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));

  observer->onNext(6);
  observer->onNext(7);
  observer->onNext(8);

  EXPECT_FALSE(observer->isUnsubscribedOrTerminated());
  EXPECT_FALSE(subscription->isCancelled());

  EXPECT_CALL(*subscriber, onError_(_))
      .WillOnce(Invoke([&](folly::exception_wrapper ex) {
        EXPECT_TRUE(ex.is_compatible_with<
                    yarpl::flowable::MissingBackpressureException>());
      }));

  observer->onNext(9);

  EXPECT_TRUE(observer->isUnsubscribedOrTerminated());
  EXPECT_TRUE(subscription->isCancelled());
}

TEST(Observable, toFlowableBufferStrategyStress) {
  std::shared_ptr<Observer<int64_t>> observer;
  auto a = Observable<int64_t>::createEx(
      [&](auto o, auto) { observer = std::move(o); });
  auto f = a->toFlowable(BackpressureStrategy::BUFFER);

  std::vector<int64_t> v;
  std::atomic<int64_t> tokens{0};

  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(0);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);
  EXPECT_TRUE(observer);

  constexpr size_t kNumElements = 100000;

  std::thread nextThread([&] {
    for (size_t i = 0; i < kNumElements; ++i) {
      while (tokens.load() < -5) {
        std::this_thread::yield();
      }

      observer->onNext(i);
      --tokens;
    }
    observer->onComplete();
  });

  std::thread requestThread([&] {
    for (size_t i = 0; i < kNumElements; ++i) {
      while (tokens.load() > 5) {
        std::this_thread::yield();
      }

      subscriber->subscription()->request(1);
      ++tokens;
    }
  });

  nextThread.join();
  requestThread.join();

  for (size_t i = 0; i < kNumElements; ++i) {
    CHECK_EQ(i, v[i]);
  }
}

TEST(Observable, toFlowableLatestStrategy) {
  auto a = Observable<>::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::LATEST);

  std::vector<int64_t> v;

  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));

  subscriber->subscription()->request(5);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5, 10}));
}

TEST(Observable, Just) {
  EXPECT_EQ(run(Observable<>::just(22)), std::vector<int>{22});
  EXPECT_EQ(
      run(Observable<>::justN({12, 34, 56, 98})),
      std::vector<int>({12, 34, 56, 98}));
  EXPECT_EQ(
      run(Observable<>::justN({"ab", "pq", "yz"})),
      std::vector<const char*>({"ab", "pq", "yz"}));
}

TEST(Observable, SingleMovable) {
  auto value = std::make_unique<int>(123456);

  auto observable = Observable<>::justOnce(std::move(value));
  EXPECT_EQ(std::size_t{1}, observable.use_count());

  auto values = run(std::move(observable));
  EXPECT_EQ(values.size(), size_t(1));

  EXPECT_EQ(*values[0], 123456);
}

TEST(Observable, MapWithException) {
  auto observable = Observable<>::justN<int>({1, 2, 3, 4})->map([](int n) {
    if (n > 2) {
      throw std::runtime_error{"Too big!"};
    }
    return n;
  });

  auto observer = std::make_shared<CollectingObserver<int>>();
  observable->subscribe(observer);

  EXPECT_EQ(observer->values(), std::vector<int>({1, 2}));
  EXPECT_TRUE(observer->error());
  EXPECT_EQ(observer->errorMsg(), "Too big!");
}

TEST(Observable, Range) {
  auto observable = Observable<>::range(10, 4);
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({10, 11, 12, 13}));
}

TEST(Observable, RangeWithMap) {
  auto observable = Observable<>::range(1, 3)
                        ->map([](int64_t v) { return v * v; })
                        ->map([](int64_t v) { return v * v; })
                        ->map([](int64_t v) { return std::to_string(v); });
  EXPECT_EQ(
      run(std::move(observable)), std::vector<std::string>({"1", "16", "81"}));
}

TEST(Observable, RangeWithReduce) {
  auto observable = Observable<>::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({45}));
}

TEST(Observable, RangeWithReduceByMultiplication) {
  auto observable = Observable<>::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc * v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({0}));

  observable = Observable<>::range(1, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc * v; });
  EXPECT_EQ(
      run(std::move(observable)),
      std::vector<int64_t>({1 * 2 * 3 * 4 * 5 * 6 * 7 * 8 * 9 * 10}));
}

TEST(Observable, RangeWithReduceOneItem) {
  auto observable = Observable<>::range(5, 1)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({5}));
}

TEST(Observable, RangeWithReduceNoItem) {
  auto observable = Observable<>::range(0, 0)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  auto collector = std::make_shared<CollectingObserver<int64_t>>();
  observable->subscribe(collector);
  EXPECT_EQ(collector->error(), false);
  EXPECT_EQ(collector->values(), std::vector<int64_t>({}));
}

TEST(Observable, RangeWithReduceToBiggerType) {
  auto observable =
      Observable<>::range(5, 1)
          ->map([](int64_t v) { return (int32_t)v; })
          ->reduce([](int64_t acc, int32_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({5}));
}

TEST(Observable, StringReduce) {
  auto observable =
      Observable<>::justN<std::string>(
          {"a", "b", "c", "d", "e", "f", "g", "h", "i"})
          ->reduce([](std::string acc, std::string v) { return acc + v; });
  EXPECT_EQ(
      run(std::move(observable)), std::vector<std::string>({"abcdefghi"}));
}

TEST(Observable, RangeWithFilter) {
  auto observable =
      Observable<>::range(0, 10)->filter([](int64_t v) { return v % 2 != 0; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({1, 3, 5, 7, 9}));
}

TEST(Observable, SimpleTake) {
  EXPECT_EQ(
      run(Observable<>::range(0, 100)->take(3)),
      std::vector<int64_t>({0, 1, 2}));

  EXPECT_EQ(
      run(Observable<>::range(0, 100)->take(0)), std::vector<int64_t>({}));
}

TEST(Observable, TakeError) {
  auto take0 =
      Observable<int64_t>::error(std::runtime_error("something broke!"))
          ->take(0);

  auto collector = std::make_shared<CollectingObserver<int64_t>>();
  take0->subscribe(collector);

  EXPECT_EQ(collector->values(), std::vector<int64_t>({}));
  EXPECT_TRUE(collector->complete());
  EXPECT_FALSE(collector->error());
}

TEST(Observable, SimpleSkip) {
  EXPECT_EQ(
      run(Observable<>::range(0, 10)->skip(8)), std::vector<int64_t>({8, 9}));
}

TEST(Observable, OverflowSkip) {
  EXPECT_EQ(
      run(Observable<>::range(0, 10)->skip(12)), std::vector<int64_t>({}));
}

TEST(Observable, IgnoreElements) {
  auto collector = std::make_shared<CollectingObserver<int64_t>>();
  auto observable = Observable<>::range(0, 105)->ignoreElements()->map(
      [](int64_t v) { return v + 1; });
  observable->subscribe(collector);

  EXPECT_EQ(collector->values(), std::vector<int64_t>({}));
  EXPECT_EQ(collector->complete(), true);
  EXPECT_EQ(collector->error(), false);
}

TEST(Observable, Error) {
  auto observable =
      Observable<int>::error(std::runtime_error("something broke!"));
  auto collector = std::make_shared<CollectingObserver<int>>();
  observable->subscribe(collector);

  EXPECT_EQ(collector->complete(), false);
  EXPECT_EQ(collector->error(), true);
  EXPECT_EQ(collector->errorMsg(), "something broke!");
}

TEST(Observable, ErrorPtr) {
  auto observable =
      Observable<int>::error(std::runtime_error("something broke!"));
  auto collector = std::make_shared<CollectingObserver<int>>();
  observable->subscribe(collector);

  EXPECT_EQ(collector->complete(), false);
  EXPECT_EQ(collector->error(), true);
  EXPECT_EQ(collector->errorMsg(), "something broke!");
}

TEST(Observable, Empty) {
  auto observable = Observable<int>::empty();
  auto collector = std::make_shared<CollectingObserver<int>>();
  observable->subscribe(collector);

  EXPECT_EQ(collector->complete(), true);
  EXPECT_EQ(collector->error(), false);
}

TEST(Observable, ObserversComplete) {
  auto observable = Observable<int>::empty();
  bool completed = false;

  auto observer = Observer<int>::create(
      [](int) { unreachable(); },
      [](folly::exception_wrapper) { unreachable(); },
      [&] { completed = true; });

  observable->subscribe(std::move(observer));
  EXPECT_TRUE(completed);
}

TEST(Observable, ObserversError) {
  auto observable = Observable<int>::error(std::runtime_error("Whoops"));
  bool errored = false;

  auto observer = Observer<int>::create(
      [](int) { unreachable(); },
      [&](folly::exception_wrapper) { errored = true; },
      [] { unreachable(); });

  observable->subscribe(std::move(observer));
  EXPECT_TRUE(errored);
}

TEST(Observable, CancelReleasesObjects) {
  auto lambda = [](std::shared_ptr<Observer<int>> observer) {
    // we will send nothing
  };
  auto observable = Observable<int>::create(std::move(lambda));

  auto collector = std::make_shared<CollectingObserver<int>>();
  observable->subscribe(collector);
}

TEST(Observable, CompleteReleasesObjects) {
  auto shared = std::make_shared<std::shared_ptr<Observer<int>>>();
  {
    auto observable = Observable<int>::create(
                          [shared](std::shared_ptr<Observer<int>> observer) {
                            *shared = observer;
                            // onComplete releases the DoOnComplete operator
                            // so the lambda params will be freed
                            observer->onComplete();
                          })
                          ->doOnComplete([shared] {});
    observable->subscribe();
  }
  EXPECT_EQ(1, shared->use_count());
}

TEST(Observable, ErrorReleasesObjects) {
  auto shared = std::make_shared<std::shared_ptr<Observer<int>>>();
  {
    auto observable = Observable<int>::create(
                          [shared](std::shared_ptr<Observer<int>> observer) {
                            *shared = observer;
                            // onError releases the DoOnComplete operator
                            // so the lambda params will be freed
                            observer->onError(std::runtime_error("error"));
                          })
                          ->doOnComplete([shared] { /*never executed*/ });
    observable->subscribe();
  }
  EXPECT_EQ(1, shared->use_count());
}

class InfiniteAsyncTestOperator : public ObservableOperator<int, int> {
  using Super = ObservableOperator<int, int>;

 public:
  InfiniteAsyncTestOperator(
      std::shared_ptr<Observable<int>> upstream,
      MockFunction<void()>& checkpoint)
      : upstream_(std::move(upstream)), checkpoint_(checkpoint) {}

  std::shared_ptr<Subscription> subscribe(
      std::shared_ptr<Observer<int>> observer) override {
    auto subscription =
        std::make_shared<TestSubscription>(std::move(observer), checkpoint_);
    upstream_->subscribe(
        // Note: implicit cast to a reference to a observer.
        subscription);
    return subscription;
  }

 private:
  class TestSubscription : public Super::OperatorSubscription {
    using SuperSub = typename Super::OperatorSubscription;

   public:
    ~TestSubscription() {
      t_.join();
    }

    void sendSuperNext() {
      // workaround for gcc bug 58972.
      SuperSub::observerOnNext(1);
    }

    TestSubscription(
        std::shared_ptr<Observer<int>> observer,
        MockFunction<void()>& checkpoint)
        : SuperSub(std::move(observer)), checkpoint_(checkpoint) {}

    void onSubscribe(std::shared_ptr<Subscription> subscription) override {
      SuperSub::onSubscribe(std::move(subscription));
      t_ = std::thread([this]() {
        while (!isCancelled()) {
          sendSuperNext();
        }
        checkpoint_.Call();
      });
    }
    void onNext(int /*value*/) override {}

    std::thread t_;
    MockFunction<void()>& checkpoint_;
  };

  std::shared_ptr<Observable<int>> upstream_;
  MockFunction<void()>& checkpoint_;
};

// FIXME: This hits an ASAN heap-use-after-free.  Disabling for now, but we need
// to get back to this and fix it.
TEST(Observable, DISABLED_CancelSubscriptionChain) {
  std::atomic_int emitted{0};
  std::mutex m;
  std::condition_variable cv;

  MockFunction<void()> checkpoint;
  MockFunction<void()> checkpoint2;
  MockFunction<void()> checkpoint3;
  std::thread t;
  auto infinite1 =
      Observable<int>::create([&](std::shared_ptr<Observer<int>> obs) {
        EXPECT_CALL(checkpoint, Call()).Times(1);
        EXPECT_CALL(checkpoint2, Call()).Times(1);
        EXPECT_CALL(checkpoint3, Call()).Times(1);
        t = std::thread([obs, &emitted, &checkpoint]() {
          while (!obs->isUnsubscribed()) {
            ++emitted;
            obs->onNext(0);
          }
          checkpoint.Call();
        });
      });
  auto infinite2 = infinite1->skip(1)->skip(1);
  auto test1 =
      std::make_shared<InfiniteAsyncTestOperator>(infinite2, checkpoint2);
  auto test2 =
      std::make_shared<InfiniteAsyncTestOperator>(test1->skip(1), checkpoint3);
  auto skip = test2->skip(8);

  auto subscription = skip->subscribe([](int) {});

  std::unique_lock<std::mutex> lk(m);
  CHECK(cv.wait_for(
      lk, std::chrono::seconds(1), [&] { return emitted >= 1000; }));

  subscription->cancel();
  t.join();

  LOG(INFO) << "cancelled after " << emitted << " items";
}

TEST(Observable, DoOnSubscribeTest) {
  auto a = Observable<int>::empty();

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnSubscribe([&] { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnNextTest) {
  std::vector<int64_t> values;
  auto observable = Observable<>::range(10, 14)->doOnNext(
      [&](int64_t v) { values.push_back(v); });
  auto values2 = run(std::move(observable));
  EXPECT_EQ(values, values2);
}

TEST(Observable, DoOnErrorTest) {
  auto a = Observable<int>::error(std::runtime_error("something broke!"));

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnError([&](const auto&) { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnTerminateTest) {
  auto a = Observable<int>::empty();

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnTerminate([&]() { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnTerminate2Test) {
  auto a = Observable<int>::error(std::runtime_error("something broke!"));

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnTerminate([&]() { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnEachTest) {
  // TODO(lehecka): rewrite with concatWith
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    obs->onNext(5);
    obs->onError(std::runtime_error("something broke!"));
  });

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call()).Times(2);
  a->doOnEach([&]() { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnTest) {
  // TODO(lehecka): rewrite with concatWith
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    obs->onNext(5);
    obs->onError(std::runtime_error("something broke!"));
  });

  MockFunction<void()> checkpoint1;
  EXPECT_CALL(checkpoint1, Call());
  MockFunction<void()> checkpoint2;
  EXPECT_CALL(checkpoint2, Call());

  a->doOn(
       [&](int value) {
         checkpoint1.Call();
         EXPECT_EQ(value, 5);
       },
       [] { FAIL(); },
       [&](const auto&) { checkpoint2.Call(); })
      ->subscribe();
}

TEST(Observable, DoOnCancelTest) {
  auto a = Observable<>::range(1, 10);

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnCancel([&]() { checkpoint.Call(); })->take(1)->subscribe();
}

TEST(Observable, DeferTest) {
  int switchValue = 0;
  auto observable = Observable<int64_t>::defer([&]() {
    if (switchValue == 0) {
      return Observable<>::range(1, 1);
    } else {
      return Observable<>::range(3, 1);
    }
  });

  EXPECT_EQ(run(observable), std::vector<int64_t>({1}));
  switchValue = 1;
  EXPECT_EQ(run(observable), std::vector<int64_t>({3}));
}

TEST(Observable, DeferExceptionTest) {
  auto observable =
      Observable<int>::defer([&]() -> std::shared_ptr<Observable<int>> {
        throw std::runtime_error{"Too big!"};
      });

  auto observer = std::make_shared<CollectingObserver<int>>();
  observable->subscribe(observer);

  EXPECT_TRUE(observer->error());
  EXPECT_EQ(observer->errorMsg(), "Too big!");
}

TEST(Observable, ConcatWithTest) {
  auto first = Observable<>::range(1, 2);
  auto second = Observable<>::range(5, 2);
  auto combined = first->concatWith(second);

  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6}));
  // Subscribe again
  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6}));
}

TEST(Observable, ConcatWithMultipleTest) {
  auto first = Observable<>::range(1, 2);
  auto second = Observable<>::range(5, 2);
  auto third = Observable<>::range(10, 2);
  auto fourth = Observable<>::range(15, 2);
  auto firstSecond = first->concatWith(second);
  auto thirdFourth = third->concatWith(fourth);
  auto combined = firstSecond->concatWith(thirdFourth);

  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6, 10, 11, 15, 16}));
}

TEST(Observable, ConcatWithExceptionTest) {
  auto first = Observable<>::range(1, 2);
  auto second = Observable<>::range(5, 2);
  auto third = Observable<int64_t>::error(std::runtime_error("error"));

  auto combined = first->concatWith(second)->concatWith(third);

  auto observer = std::make_shared<CollectingObserver<int64_t>>();
  combined->subscribe(observer);

  EXPECT_EQ(observer->values(), std::vector<int64_t>({1, 2, 5, 6}));
  EXPECT_TRUE(observer->error());
  EXPECT_EQ(observer->errorMsg(), "error");
}

TEST(Observable, ConcatWithCancelTest) {
  auto first = Observable<>::range(1, 2);
  auto second = Observable<>::range(5, 2);
  auto combined = first->concatWith(second);
  auto take0 = combined->take(0);

  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6}));
  EXPECT_EQ(run(take0), std::vector<int64_t>({}));
}

TEST(Observable, ConcatWithCompleteAtSubscription) {
  auto first = Observable<>::range(1, 2);
  auto second = Observable<>::range(5, 2);

  auto combined = first->concatWith(second)->take(0);
  EXPECT_EQ(run(combined), std::vector<int64_t>({}));
}

TEST(Observable, ConcatWithVarArgsTest) {
  auto first = Observable<>::range(1, 2);
  auto second = Observable<>::range(5, 2);
  auto third = Observable<>::range(10, 2);
  auto fourth = Observable<>::range(15, 2);

  auto combined = first->concatWith(second, third, fourth);
  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6, 10, 11, 15, 16}));
}

TEST(Observable, ConcatTest) {
  auto combined = Observable<int64_t>::concat(
      Observable<>::range(1, 2), Observable<>::range(5, 2));
  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6}));

  // Observable::concat shoud not accept one parameter!
  // Next line should cause compiler failure: OK!
  // combined = Observable<int64_t>::concat(Observable<>::range(1, 2));

  combined = Observable<int64_t>::concat(
      Observable<>::range(1, 2),
      Observable<>::range(5, 2),
      Observable<>::range(10, 2));
  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6, 10, 11}));

  combined = Observable<int64_t>::concat(
      Observable<>::range(1, 2),
      Observable<>::range(5, 2),
      Observable<>::range(10, 2),
      Observable<>::range(15, 2));
  EXPECT_EQ(run(combined), std::vector<int64_t>({1, 2, 5, 6, 10, 11, 15, 16}));
}

TEST(Observable, ToFlowableConcat) {
  // Concat a flowable with an observable.
  // Convert the observable to flowable before concat.
  // Use ERROR as backpressure strategy.

  // Test: Request only as much as the initial flowable provides
  //   - Check that the observable is not subscribed to so it doesn't flood

  auto a = yarpl::flowable::Flowable<>::range(1, 1);
  auto b = Observable<>::range(2, 9)->toFlowable(BackpressureStrategy::ERROR);

  auto c = a->concatWith(b);

  uint32_t request = 1;
  auto subscriber =
      std::make_shared<testing::StrictMock<MockSubscriber<int64_t>>>(request);

  std::vector<int64_t> v;

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onError_(_)).Times(0);

  c->subscribe(subscriber);

  // As only 1 item is requested, the second flowable will not be subscribed. So
  // the observer will not flood the stream and cause ERROR.
  EXPECT_EQ(v, std::vector<int64_t>({1}));

  // Now flood the stream
  EXPECT_CALL(*subscriber, onError_(_));
  subscriber->subscription()->request(1);
}
