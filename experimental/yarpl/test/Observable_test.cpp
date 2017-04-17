// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "yarpl/Observable.h"
#include "yarpl/Tuple.h"

using namespace yarpl::observable;

TEST(Observable, SingleOnNext) {
  auto a =
      Observables::unsafeCreate<int>([](std::unique_ptr<Observer<int>> obs) {
        auto s = Subscriptions::create();
        obs->onSubscribe(s.get());
        obs->onNext(1);
        obs->onComplete();
      });

  std::vector<int> v;
  a->subscribe(Observers::create<int>([&v](int value) { v.push_back(value); }));

  EXPECT_EQ(v.at(0), 1);
}

TEST(Observable, MultiOnNext) {
  auto a =
      Observables::unsafeCreate<int>([](std::unique_ptr<Observer<int>> obs) {
        auto s = Subscriptions::create();
        obs->onSubscribe(s.get());
        obs->onNext(1);
        obs->onNext(2);
        obs->onNext(3);
        obs->onComplete();
      });

  std::vector<int> v;
  a->subscribe(Observers::create<int>([&v](int value) { v.push_back(value); }));

  EXPECT_EQ(v.at(0), 1);
  EXPECT_EQ(v.at(1), 2);
  EXPECT_EQ(v.at(2), 3);
}

TEST(Observable, OnError) {
  std::string errorMessage("DEFAULT->No Error Message");
  auto a =
      Observables::unsafeCreate<int>([](std::unique_ptr<Observer<int>> obs) {
        try {
          throw std::runtime_error("something broke!");
        } catch (const std::exception&) {
          obs->onError(std::current_exception());
        }
      });

  a->subscribe(Observers::create<int>(
      [](int value) { /* do nothing */ },
      [&errorMessage](const std::exception_ptr e) {
        try {
          std::rethrow_exception(e);
        } catch (const std::runtime_error& ex) {
          errorMessage = std::string(ex.what());
        }
      }));

  EXPECT_EQ("something broke!", errorMessage);
}

static std::atomic<int> instanceCount;

/**
 * Assert that all items passed through the Observable get destroyed
 */
TEST(Observable, ItemsCollectedSynchronously) {
  auto a = Observables::unsafeCreate<Tuple>(
      [](std::unique_ptr<Observer<Tuple>> obs) {
        auto s = Subscriptions::create();
        obs->onSubscribe(s.get());
        obs->onNext(Tuple{1, 2});
        obs->onNext(Tuple{2, 3});
        obs->onNext(Tuple{3, 4});
        obs->onComplete();
      });

  // TODO how can it be made so 'auto' correctly works without doing copying?
  // a->subscribe(Observer<Tuple>::create(
  //    [](auto value) { std::cout << "received value " << value.a << "\n"; }));
  a->subscribe(Observers::create<Tuple>([](const Tuple& value) {
    std::cout << "received value " << value.a << std::endl;
  }));

  std::cout << "Finished ... remaining instances == " << instanceCount
            << std::endl;

  EXPECT_EQ(0, Tuple::instanceCount);
  std::cout << "-----------------------------" << std::endl;
}

/*
 * Assert that all items passed through the Flowable get
 * copied and destroyed correctly over async boundaries.
 *
 * This is simulating "async" by having an Observer store the items
 * in a Vector which could then be consumed on another thread.
 */
TEST(Observable, ItemsCollectedAsynchronously) {
  // scope this so we can check destruction of Vector after this block
  {
    auto a = Observables::unsafeCreate<Tuple>(
        [](std::unique_ptr<Observer<Tuple>> obs) {
          auto s = Subscriptions::create();
          obs->onSubscribe(s.get());
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

  EXPECT_EQ(0, Tuple::instanceCount);
  std::cout << "-----------------------------" << std::endl;
}

class TakeObserver : public Observer<int> {
 private:
  const int limit;
  int count = 0;
  Subscription* subscription_;
  std::vector<int>& v;

 public:
  TakeObserver(int _limit, std::vector<int>& _v) : limit(_limit), v(_v) {
    v.reserve(5);
  }

  void onSubscribe(Subscription* s) override {
    subscription_ = s;
  }

  void onNext(const int& value) override {
    v.push_back(value);
    if (++count >= limit) {
      //      std::cout << "Cancelling subscription after receiving " << count
      //                << " items." << std::endl;
      subscription_->cancel();
    }
  }

  void onError(const std::exception_ptr e) override {}
  void onComplete() override {}
};

// assert behavior of onComplete after subscription.cancel
TEST(Observable, SubscriptionCancellation) {
  static std::atomic_int emitted{0};
  auto a =
      Observables::unsafeCreate<int>([](std::unique_ptr<Observer<int>> obs) {
        std::atomic_bool isUnsubscribed{false};
        auto s =
            Subscriptions::create([&isUnsubscribed] { isUnsubscribed = true; });
        obs->onSubscribe(s.get());
        int i = 0;
        while (!isUnsubscribed && i <= 10) {
          emitted++;
          obs->onNext(i++);
        }
        if (!isUnsubscribed) {
          obs->onComplete();
        }
      });

  std::vector<int> v;
  a->subscribe(std::make_unique<TakeObserver>(2, v));
  EXPECT_EQ((unsigned long)2, v.size());
  EXPECT_EQ(2, emitted);
}
