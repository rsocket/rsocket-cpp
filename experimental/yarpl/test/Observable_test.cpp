// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include "Tuple.h"
#include "yarpl/Observable.h"
#include "yarpl/observable/Observers.h"
#include "yarpl/observable/Subscriptions.h"

// TODO can we eliminate need to import both of these?
using namespace yarpl;
using namespace yarpl::observable;

TEST(Observable, SingleOnNext) {
  {
    ASSERT_EQ(std::size_t{0}, Refcounted::objects());
    auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
      auto s = Subscriptions::empty();
      obs->onSubscribe(s);
      obs->onNext(1);
      obs->onComplete();
    });

    ASSERT_EQ(std::size_t{1}, Refcounted::objects());

    std::vector<int> v;
    a->subscribe(
        Observers::create<int>([&v](const int& value) { v.push_back(value); }));

    ASSERT_EQ(std::size_t{1}, Refcounted::objects());
    EXPECT_EQ(v.at(0), 1);
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

TEST(Observable, MultiOnNext) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  {
    auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
      obs->onSubscribe(Subscriptions::empty());
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
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

TEST(Observable, OnError) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  {
    std::string errorMessage("DEFAULT->No Error Message");
    auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
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
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

static std::atomic<int> instanceCount;

/**
 * Assert that all items passed through the Observable get destroyed
 */
TEST(Observable, ItemsCollectedSynchronously) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  {
    auto a = Observable<Tuple>::create([](Reference<Observer<Tuple>> obs) {
      obs->onSubscribe(Subscriptions::empty());
      obs->onNext(Tuple{1, 2});
      obs->onNext(Tuple{2, 3});
      obs->onNext(Tuple{3, 4});
      obs->onComplete();
    });

    a->subscribe(Observers::create<Tuple>([](const Tuple& value) {
      std::cout << "received value " << value.a << std::endl;
    }));

    std::cout << "Finished ... remaining instances == " << instanceCount
              << std::endl;

    EXPECT_EQ(0, Tuple::instanceCount);
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

/*
 * Assert that all items passed through the Observable get
 * copied and destroyed correctly over async boundaries.
 *
 * This is simulating "async" by having an Observer store the items
 * in a Vector which could then be consumed on another thread.
 */
TEST(DISABLED_Observable, ItemsCollectedAsynchronously) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  // scope this so we can check destruction of Vector after this block
  {
    auto a = Observable<Tuple>::create([](Reference<Observer<Tuple>> obs) {
      obs->onSubscribe(Subscriptions::empty());
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
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
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

  void onError(const std::exception_ptr e) override {}
  void onComplete() override {}
};

// assert behavior of onComplete after subscription.cancel
TEST(Observable, SubscriptionCancellation) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  {
    static std::atomic_int emitted{0};
    auto a = Observable<int>::create([](Reference<Observer<int>> obs) {
      std::atomic_bool isUnsubscribed{false};
      auto s =
          Subscriptions::create([&isUnsubscribed] { isUnsubscribed = true; });
      obs->onSubscribe(std::move(s));
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
    a->subscribe(Reference<Observer<int>>(new TakeObserver(2, v)));
    EXPECT_EQ((unsigned long)2, v.size());
    EXPECT_EQ(2, emitted);
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}
