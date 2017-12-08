// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/synchronization/Baton.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <condition_variable>

#include "yarpl/Observable.h"
#include "yarpl/flowable/Subscriber.h"
#include "yarpl/flowable/Subscribers.h"
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
  }

  void onError(folly::exception_wrapper ex) override {
    Observer<T>::onError(ex);
    error_ = true;
    errorMsg_ = ex.get_exception()->what();
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

 private:
  std::vector<T> values_;
  std::string errorMsg_;
  bool complete_{false};
  bool error_{false};
};

/// Construct a pipeline with a collecting observer against the supplied
/// observable.  Return the items that were sent to the observer.  If some
/// exception was sent, the exception is thrown.
template <typename T>
std::vector<T> run(Reference<Observable<T>> observable) {
  auto collector = make_ref<CollectingObserver<T>>();
  observable->subscribe(collector);
  return std::move(collector->values());
}

} // namespace

TEST(Observable, SingleOnNext) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    obs->onNext(1);
    obs->onComplete();
  });

  std::vector<int> v;
  a->subscribe(
      Observers::create<int>([&v](const int& value) { v.push_back(value); }));
  EXPECT_EQ(v.at(0), 1);
}

TEST(Observable, MultiOnNext) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    obs->onNext(1);
    obs->onNext(2);
    obs->onNext(3);
    obs->onComplete();
  });

  std::vector<int> v;
  a->subscribe(
      Observers::create<int>([&v](const int& value) { v.push_back(value); }));

  EXPECT_EQ(v.at(0), 1);
  EXPECT_EQ(v.at(1), 2);
  EXPECT_EQ(v.at(2), 3);
}

TEST(Observable, OnError) {
  std::string errorMessage("DEFAULT->No Error Message");
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    obs->onError(std::runtime_error("something broke!"));
  });

  a->subscribe(Observers::create<int>(
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
  auto a = Observable<Tuple>::create([](Reference<Observer<Tuple>> obs) {
    obs->onNext(Tuple{1, 2});
    obs->onNext(Tuple{2, 3});
    obs->onNext(Tuple{3, 4});
    obs->onComplete();
  });

  a->subscribe(Observers::create<Tuple>([](const Tuple& value) {
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
  auto a = Observable<Tuple>::create([](Reference<Observer<Tuple>> obs) {
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
  a->subscribe(Observers::create<Tuple>([&v](const Tuple& value) {
    std::cout << "received value " << value.a << std::endl;
    // copy into vector
    v.push_back(value);
    std::cout << "done pushing into vector" << std::endl;
  }));

  // expect that 3 instances were originally created, then 3 more when copying
  EXPECT_EQ(6, Tuple::createdCount);
  // expect that 3 instances still exist in the vector, so only 3 destroyed so
  // far
  EXPECT_EQ(3, Tuple::destroyedCount);

  std::cout << "Leaving block now so Vector should release Tuples..."
            << std::endl;
}

class TakeObserver : public Observer<int> {
 private:
  const int limit;
  int count = 0;
  Reference<yarpl::observable::Subscription> subscription_;
  std::vector<int>& v;

 public:
  TakeObserver(int _limit, std::vector<int>& _v) : limit(_limit), v(_v) {
    v.reserve(5);
  }

  void onSubscribe(Reference<yarpl::observable::Subscription> s) override {
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
  auto a = Observable<int>::create([&](Reference<Observer<int>> obs) {
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
  a->subscribe(make_ref<TakeObserver>(2, v));
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
  auto a = Observable<int>::create([&](Reference<Observer<int>> obs) {
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
  auto a = Observables::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::DROP);

  std::vector<int64_t> v;

  auto subscriber = make_ref<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);

  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));
}

TEST(Observable, toFlowableDropWithCancel) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    int i = 0;
    while (!obs->isUnsubscribed()) {
      obs->onNext(++i);
    }
  });

  auto f = a->toFlowable(BackpressureStrategy::DROP);

  std::vector<int> v;
  f->take(5)->subscribe(yarpl::flowable::Subscribers::create<int>(
      [&v](const int& value) { v.push_back(value); }));

  EXPECT_EQ(v, std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(Observable, toFlowableErrorStrategy) {
  auto a = Observables::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::ERROR);

  std::vector<int64_t> v;

  auto subscriber = make_ref<testing::StrictMock<MockSubscriber<int64_t>>>(5);

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
  auto a = Observables::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::BUFFER);

  std::vector<int64_t> v;

  auto subscriber = make_ref<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));

  subscriber->subscription()->request(5);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

TEST(Observable, toFlowableLatestStrategy) {
  auto a = Observables::range(1, 10);
  auto f = a->toFlowable(BackpressureStrategy::LATEST);

  std::vector<int64_t> v;

  auto subscriber = make_ref<testing::StrictMock<MockSubscriber<int64_t>>>(5);

  EXPECT_CALL(*subscriber, onSubscribe_(_));
  EXPECT_CALL(*subscriber, onNext_(_))
      .WillRepeatedly(Invoke([&](int64_t value) { v.push_back(value); }));
  EXPECT_CALL(*subscriber, onComplete_());

  f->subscribe(subscriber);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5}));

  subscriber->subscription()->request(5);
  EXPECT_EQ(v, std::vector<int64_t>({1, 2, 3, 4, 5, 9}));
}

TEST(Observable, Just) {
  EXPECT_EQ(run(Observables::just(22)), std::vector<int>{22});
  EXPECT_EQ(
      run(Observables::justN({12, 34, 56, 98})),
      std::vector<int>({12, 34, 56, 98}));
  EXPECT_EQ(
      run(Observables::justN({"ab", "pq", "yz"})),
      std::vector<const char*>({"ab", "pq", "yz"}));
}

TEST(Observable, SingleMovable) {
  auto value = std::make_unique<int>(123456);

  auto observable = Observables::justOnce(std::move(value));
  EXPECT_EQ(std::size_t{1}, observable.use_count());

  auto values = run(std::move(observable));
  EXPECT_EQ(values.size(), size_t(1));

  EXPECT_EQ(*values[0], 123456);
}

TEST(Observable, MapWithException) {
  auto observable = Observables::justN<int>({1, 2, 3, 4})->map([](int n) {
    if (n > 2) {
      throw std::runtime_error{"Too big!"};
    }
    return n;
  });

  auto observer = yarpl::make_ref<CollectingObserver<int>>();
  observable->subscribe(observer);

  EXPECT_EQ(observer->values(), std::vector<int>({1, 2}));
  EXPECT_TRUE(observer->error());
  EXPECT_EQ(observer->errorMsg(), "Too big!");
}

TEST(Observable, Range) {
  auto observable = Observables::range(10, 14);
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({10, 11, 12, 13}));
}

TEST(Observable, RangeWithMap) {
  auto observable = Observables::range(1, 4)
                        ->map([](int64_t v) { return v * v; })
                        ->map([](int64_t v) { return v * v; })
                        ->map([](int64_t v) { return std::to_string(v); });
  EXPECT_EQ(
      run(std::move(observable)), std::vector<std::string>({"1", "16", "81"}));
}

TEST(Observable, RangeWithReduce) {
  auto observable = Observables::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({45}));
}

TEST(Observable, RangeWithReduceByMultiplication) {
  auto observable = Observables::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc * v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({0}));

  observable = Observables::range(1, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc * v; });
  EXPECT_EQ(
      run(std::move(observable)),
      std::vector<int64_t>({2 * 3 * 4 * 5 * 6 * 7 * 8 * 9}));
}

TEST(Observable, RangeWithReduceOneItem) {
  auto observable = Observables::range(5, 6)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({5}));
}

TEST(Observable, RangeWithReduceNoItem) {
  auto observable = Observables::range(0, 0)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  auto collector = make_ref<CollectingObserver<int64_t>>();
  observable->subscribe(collector);
  EXPECT_EQ(collector->error(), false);
  EXPECT_EQ(collector->values(), std::vector<int64_t>({}));
}

TEST(Observable, RangeWithReduceToBiggerType) {
  auto observable =
      Observables::range(5, 6)
          ->map([](int64_t v) { return (int32_t)v; })
          ->reduce([](int64_t acc, int32_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({5}));
}

TEST(Observable, StringReduce) {
  auto observable =
      Observables::justN<std::string>(
          {"a", "b", "c", "d", "e", "f", "g", "h", "i"})
          ->reduce([](std::string acc, std::string v) { return acc + v; });
  EXPECT_EQ(
      run(std::move(observable)), std::vector<std::string>({"abcdefghi"}));
}

TEST(Observable, RangeWithFilter) {
  auto observable =
      Observables::range(0, 10)->filter([](int64_t v) { return v % 2 != 0; });
  EXPECT_EQ(run(std::move(observable)), std::vector<int64_t>({1, 3, 5, 7, 9}));
}

TEST(Observable, SimpleTake) {
  EXPECT_EQ(
      run(Observables::range(0, 100)->take(3)),
      std::vector<int64_t>({0, 1, 2}));
}

TEST(Observable, SimpleSkip) {
  EXPECT_EQ(
      run(Observables::range(0, 10)->skip(8)), std::vector<int64_t>({8, 9}));
}

TEST(Observable, OverflowSkip) {
  EXPECT_EQ(run(Observables::range(0, 10)->skip(12)), std::vector<int64_t>({}));
}

TEST(Observable, IgnoreElements) {
  auto collector = make_ref<CollectingObserver<int64_t>>();
  auto observable = Observables::range(0, 105)->ignoreElements()->map(
      [](int64_t v) { return v + 1; });
  observable->subscribe(collector);

  EXPECT_EQ(collector->values(), std::vector<int64_t>({}));
  EXPECT_EQ(collector->complete(), true);
  EXPECT_EQ(collector->error(), false);
}

TEST(Observable, Error) {
  auto observable =
      Observables::error<int>(std::runtime_error("something broke!"));
  auto collector = make_ref<CollectingObserver<int>>();
  observable->subscribe(collector);

  EXPECT_EQ(collector->complete(), false);
  EXPECT_EQ(collector->error(), true);
  EXPECT_EQ(collector->errorMsg(), "something broke!");
}

TEST(Observable, ErrorPtr) {
  auto observable =
      Observables::error<int>(std::runtime_error("something broke!"));
  auto collector = make_ref<CollectingObserver<int>>();
  observable->subscribe(collector);

  EXPECT_EQ(collector->complete(), false);
  EXPECT_EQ(collector->error(), true);
  EXPECT_EQ(collector->errorMsg(), "something broke!");
}

TEST(Observable, Empty) {
  auto observable = Observables::empty<int>();
  auto collector = make_ref<CollectingObserver<int>>();
  observable->subscribe(collector);

  EXPECT_EQ(collector->complete(), true);
  EXPECT_EQ(collector->error(), false);
}

TEST(Observable, ObserversComplete) {
  auto observable = Observables::empty<int>();
  bool completed = false;

  auto observer = Observers::create<int>(
      [](int) { unreachable(); },
      [](folly::exception_wrapper) { unreachable(); },
      [&] { completed = true; });

  observable->subscribe(std::move(observer));
  EXPECT_TRUE(completed);
}

TEST(Observable, ObserversError) {
  auto observable = Observables::error<int>(std::runtime_error("Whoops"));
  bool errored = false;

  auto observer = Observers::create<int>(
      [](int) { unreachable(); },
      [&](folly::exception_wrapper) { errored = true; },
      [] { unreachable(); });

  observable->subscribe(std::move(observer));
  EXPECT_TRUE(errored);
}

TEST(Observable, CancelReleasesObjects) {
  auto lambda = [](Reference<Observer<int>> observer) {
    // we will send nothing
  };
  auto observable = Observable<int>::create(std::move(lambda));

  auto collector = make_ref<CollectingObserver<int>>();
  observable->subscribe(collector);
}

class InfiniteAsyncTestOperator
    : public ObservableOperator<int, int, InfiniteAsyncTestOperator> {
  using Super = ObservableOperator<int, int, InfiniteAsyncTestOperator>;

 public:
  InfiniteAsyncTestOperator(
      Reference<Observable<int>> upstream,
      MockFunction<void()>& checkpoint)
      : Super(std::move(upstream)), checkpoint_(checkpoint) {}

  Reference<Subscription> subscribe(
      Reference<Observer<int>> observer) override {
    auto subscription = make_ref<TestSubscription>(
        this->ref_from_this(this), std::move(observer), checkpoint_);
    Super::upstream_->subscribe(
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
        Reference<InfiniteAsyncTestOperator> observable,
        Reference<Observer<int>> observer,
        MockFunction<void()>& checkpoint)
        : SuperSub(std::move(observable), std::move(observer)),
          checkpoint_(checkpoint) {}

    void onSubscribe(yarpl::Reference<Subscription> subscription) override {
      SuperSub::onSubscribe(std::move(subscription));
      t_ = std::thread([this]() {
        while (!isCancelled()) {
          sendSuperNext();
        }
        checkpoint_.Call();
      });
    }
    void onNext(int value) override {}

    std::thread t_;
    MockFunction<void()>& checkpoint_;
  };

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
  auto infinite1 = Observable<int>::create([&](Reference<Observer<int>> obs) {
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
  auto test1 = make_ref<InfiniteAsyncTestOperator>(infinite2, checkpoint2);
  auto test2 = make_ref<InfiniteAsyncTestOperator>(test1->skip(1), checkpoint3);
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
  auto a = Observable<int>::create(
      [](Reference<Observer<int>> obs) { obs->onComplete(); });

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnSubscribe([&] { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnNextTest) {
  std::vector<int64_t> values;
  auto observable = Observables::range(10, 14)->doOnNext(
      [&](int64_t v) { values.push_back(v); });
  auto values2 = run(std::move(observable));
  EXPECT_EQ(values, values2);
}

TEST(Observable, DoOnErrorTest) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    obs->onError(std::runtime_error("something broke!"));
  });

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnError([&](const auto&) { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnTerminateTest) {
  auto a = Observable<int>::create(
      [](Reference<Observer<int>> obs) { obs->onComplete(); });

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnTerminate([&]() { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnTerminate2Test) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    obs->onError(std::runtime_error("something broke!"));
  });

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call());

  a->doOnTerminate([&]() { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnEachTest) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
    obs->onNext(5);
    obs->onError(std::runtime_error("something broke!"));
  });

  MockFunction<void()> checkpoint;
  EXPECT_CALL(checkpoint, Call()).Times(2);
  a->doOnEach([&]() { checkpoint.Call(); })->subscribe();
}

TEST(Observable, DoOnTest) {
  auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
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
