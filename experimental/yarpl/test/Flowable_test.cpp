// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"

using namespace yarpl::flowable;
using namespace reactivestreams_yarpl;

/*
 * Verbose usage of interfaces with inline class implementations.
 *
 * - Demonstrating subscribe, onSubscribe, request, onNext, cancel.
 * - Shows use of unique_ptr and std::move to pass around the Subscriber and
 * Subscription.
 */
TEST(Flowable, SubscribeRequestAndCancel) {
  std::cout << "----------------------------- ************** HERE1"
            << std::endl;

  // Subscription that emits integers forever as long as requested
  class InfiniteIntegersSource : public Subscription<int> {
    std::unique_ptr<Subscriber<int>> subscriber_;
    std::atomic_bool isCancelled{false};

   public:
    static void subscribe(std::unique_ptr<Subscriber<int>> subscriber) {
      InfiniteIntegersSource s_(std::move(subscriber));
    }

    void start() {
      subscriber_->onSubscribe(this);
    }

    void cancel() override {
      isCancelled = true;
    }
    void request(long n) override {
      std::cout << "source 1 " << std::endl;
      for (auto i = 0; i < n; i++) {
        if (isCancelled) {
          return;
        }
        subscriber_->onNext(i);
      }
    }

   protected:
    InfiniteIntegersSource(std::unique_ptr<Subscriber<int>> subscriber)
        : subscriber_(std::move(subscriber)) {
      subscriber_->onSubscribe(this);
    }
  };

  std::cout << "----------------------------- ************** HERE2"
            << std::endl;

  // Subscriber that requests 10 items, then cancels after receiving 6
  class MySubscriber : public Subscriber<int> {
    Subscription<int>* subscription;

   public:
    void onNext(const int& value) override {
      std::cout << "received& " << value << std::endl;
      if (value == 6) {
        subscription->cancel();
      }
    }
    void onError(const std::exception_ptr e) override {}
    void onComplete() override {}
    void onSubscribe(Subscription<int>* s) override {
      std::cout << "onSubscribe in subscriber" << std::endl;
      subscription = s;
      subscription->request((long)10);
    }
  };

  std::cout << "----------------------------- ************** HERE3"
            << std::endl;

  // create Flowable around InfiniteIntegersSource
  auto a =
      Flowable<int>::create([](std::unique_ptr<Subscriber<int>> subscriber) {
        InfiniteIntegersSource::subscribe(std::move(subscriber));
      });

  a->subscribe(std::make_unique<MySubscriber>());
  std::cout << "----------------------------- ************** HERE4"
            << std::endl;
}
//
// TEST(Flowable, OnError) {
//  // Subscription that fails
//  class Source : public Subscription {
//    std::unique_ptr<Subscriber<int>> subscriber;
//    std::atomic_bool isCancelled{false};
//
//   public:
//    Source(std::unique_ptr<Subscriber<int>> subscriber)
//        : subscriber(std::move(subscriber)) {}
//
//    void cancel() override {
//      isCancelled = true;
//    }
//    void request(uint64_t n) override {
//      try {
//        throw std::runtime_error("something broke!");
//      } catch (const std::exception& e) {
//        subscriber->onError(e);
//      }
//    }
//  };
//
//  // create Flowable around Source
//  auto a = Flowable<int>::create([](
//      std::unique_ptr<Subscriber<int>> subscriber) {
//    subscriber->onSubscribe(std::make_unique<Source>(std::move(subscriber)));
//  });
//
//  std::string errorMessage("DEFAULT->No Error Message");
//  a->subscribe(Subscriber<int>::create(
//      [](int value) { /* do nothing */ },
//      [&errorMessage](const std::exception& e) {
//        errorMessage = std::string(e.what());
//      }));
//
//  EXPECT_EQ("something broke!", errorMessage);
//  std::cout << "-----------------------------" << std::endl;
//}
//
///**
// * Assert that all items passed through the Flowable get destroyed
// */
// TEST(Flowable, ItemsCollectedSynchronously) {
//  static std::atomic<int> instanceCount;
//
//  struct Tuple {
//    const int a;
//    const int b;
//
//    Tuple(const int a, const int b) : a(a), b(b) {
//      std::cout << "Tuple created!!" << std::endl;
//      instanceCount++;
//    }
//    Tuple(const Tuple& t) : a(t.a), b(t.b) {
//      std::cout << "Tuple copy constructed!!" << std::endl;
//      instanceCount++;
//    }
//    ~Tuple() {
//      std::cout << "Tuple destroyed!!" << std::endl;
//      instanceCount--;
//    }
//  };
//
//  class Source : public Subscription {
//    std::unique_ptr<Subscriber<Tuple>> subscriber;
//    std::atomic_bool isCancelled{false};
//
//   public:
//    Source(std::unique_ptr<Subscriber<Tuple>> subscriber)
//        : subscriber(std::move(subscriber)) {}
//
//    void cancel() override {
//      isCancelled = true;
//    }
//    void request(uint64_t n) override {
//      // ignoring n for tests ... DO NOT DO THIS FOR REAL
//      subscriber->onNext(Tuple{1, 2});
//      subscriber->onNext(Tuple{2, 3});
//      subscriber->onNext(Tuple{3, 4});
//      subscriber->onComplete();
//    }
//  };
//
//  // create Flowable around Source
//  auto a = Flowable<Tuple>::create([](
//      std::unique_ptr<Subscriber<Tuple>> subscriber) {
//    subscriber->onSubscribe(std::make_unique<Source>(std::move(subscriber)));
//  });
//
//  a->subscribe(Subscriber<Tuple>::create([](const Tuple& value) {
//    std::cout << "received value " << value.a << std::endl;
//  }));
//
//  std::cout << "Finished ... remaining instances == " << instanceCount
//            << std::endl;
//
//  EXPECT_EQ(0, instanceCount);
//  std::cout << "-----------------------------" << std::endl;
//}
//
///*
// * Assert that all items passed through the Flowable get
// * copied and destroyed correctly over async boundaries.
// *
// * This is simulating "async" by having an Observer store the items
// * in a Vector which could then be consumed on another thread.
// */
// TEST(Flowable, ItemsCollectedAsynchronously) {
//  static std::atomic<int> createdCount;
//  static std::atomic<int> destroyedCount;
//
//  struct Tuple {
//    const int a;
//    const int b;
//
//    Tuple(const int a, const int b) : a(a), b(b) {
//      std::cout << "Tuple " << a << " created!!" << std::endl;
//      createdCount++;
//    }
//    Tuple(const Tuple& t) : a(t.a), b(t.b) {
//      std::cout << "Tuple " << a << " copy constructed!!" << std::endl;
//      createdCount++;
//    }
//    ~Tuple() {
//      std::cout << "Tuple " << a << " destroyed!!" << std::endl;
//      destroyedCount++;
//    }
//  };
//
//  // scope this so we can check destruction of Vector after this block
//  {
//    // Subscription that fails
//    class Source : public Subscription {
//      std::unique_ptr<Subscriber<Tuple>> subscriber;
//      std::atomic_bool isCancelled{false};
//
//     public:
//      Source(std::unique_ptr<Subscriber<Tuple>> subscriber)
//          : subscriber(std::move(subscriber)) {}
//
//      void cancel() override {
//        isCancelled = true;
//      }
//      void request(uint64_t n) override {
//        // ignoring n for tests ... DO NOT DO THIS FOR REAL
//        std::cout << "-----------------------------" << std::endl;
//        subscriber->onNext(Tuple{1, 2});
//        std::cout << "-----------------------------" << std::endl;
//        subscriber->onNext(Tuple{2, 3});
//        std::cout << "-----------------------------" << std::endl;
//        subscriber->onNext(Tuple{3, 4});
//        std::cout << "-----------------------------" << std::endl;
//        subscriber->onComplete();
//      }
//    };
//
//    // create Flowable around Source
//    auto a = Flowable<Tuple>::create([](
//        std::unique_ptr<Subscriber<Tuple>> subscriber) {
//      subscriber->onSubscribe(std::make_unique<Source>(std::move(subscriber)));
//    });
//
//    std::vector<Tuple> v;
//    v.reserve(10); // otherwise it resizes and copies on each push_back
//    a->subscribe(Subscriber<Tuple>::create([&v](const Tuple& value) {
//      std::cout << "received value " << value.a << std::endl;
//      // copy into vector
//      v.push_back(value);
//      std::cout << "done pushing into vector" << std::endl;
//    }));
//
//    // expect that 3 instances were originally created, then 3 more when
//    copying
//    EXPECT_EQ(6, createdCount);
//    // expect that 3 instances still exist in the vector, so only 3 destroyed
//    so
//    // far
//    EXPECT_EQ(3, destroyedCount);
//
//    std::cout << "Leaving block now so Vector should release Tuples..."
//              << std::endl;
//  }
//
//  EXPECT_EQ(0, (createdCount - destroyedCount));
//  std::cout << "-----------------------------" << std::endl;
//}
//
///**
// * Assert that memory is not retained when a single Flowable
// * is subscribed to multiple times.
// */
// TEST(Flowable, TestRetainOnStaticAsyncFlowableWithMultipleSubscribers) {
//  static std::atomic<int> tupleInstanceCount;
//  static std::atomic<int> subscriptionInstanceCount;
//  static std::atomic<int> subscriberInstanceCount;
//
//  struct Tuple {
//    const int a;
//    const int b;
//
//    Tuple(const int a, const int b) : a(a), b(b) {
//      std::cout << "   Tuple created!!" << std::endl;
//      tupleInstanceCount++;
//    }
//    Tuple(const Tuple& t) : a(t.a), b(t.b) {
//      std::cout << "   Tuple copy constructed!!" << std::endl;
//      tupleInstanceCount++;
//    }
//    ~Tuple() {
//      std::cout << "   Tuple destroyed!!" << std::endl;
//      tupleInstanceCount--;
//    }
//  };
//
//  class MySubscriber : public Subscriber<Tuple> {
//    std::unique_ptr<Subscription> subscription;
//
//   public:
//    MySubscriber() {
//      std::cout << "   Subscriber created!!" << std::endl;
//      subscriberInstanceCount++;
//    }
//    ~MySubscriber() {
//      std::cout << "   Subscriber destroyed!!" << std::endl;
//      subscriberInstanceCount--;
//    }
//
//    void onNext(const Tuple& value) override {
//      std::cout << "   Subscriber received value " << value.a << " on thread "
//                << std::this_thread::get_id() << std::endl;
//    }
//    void onError(const std::exception& e) override {}
//    void onComplete() override {}
//    void onSubscribe(std::unique_ptr<Subscription> s) override {
//      subscription = std::move(s);
//      subscription->request((u_long)10);
//    }
//  };
//
//  class Source : public Subscription {
//    std::weak_ptr<Subscriber<Tuple>> subscriber;
//    std::atomic_bool isCancelled{false};
//
//   public:
//    Source(std::weak_ptr<Subscriber<Tuple>> subscriber)
//        : subscriber(std::move(subscriber)) {
//      std::cout << "   Subscription created!!" << std::endl;
//      subscriptionInstanceCount++;
//    }
//    ~Source() {
//      std::cout << "   Subscription destroyed!!" << std::endl;
//      subscriptionInstanceCount--;
//    }
//
//    void cancel() override {
//      isCancelled = true;
//    }
//    void request(uint64_t n) override {
//      // ignoring n for tests ... DO NOT DO THIS FOR REAL
//      auto ss = subscriber.lock();
//      if (ss) {
//        ss->onNext(Tuple{1, 2});
//        ss->onNext(Tuple{2, 3});
//        ss->onNext(Tuple{3, 4});
//        ss->onComplete();
//      } else {
//        std::cout << "!!!!!!!!!!! Subscriber already destroyed when REQUEST_N
//        "
//                     "received."
//                  << std::endl;
//      }
//    }
//  };
//
//  {
//    static std::atomic_int counter{2};
//    // create Flowable around Source
//    auto a = Flowable<Tuple>::create([](
//        std::unique_ptr<Subscriber<Tuple>> subscriber) {
//      std::cout << "Flowable subscribed to ... starting new thread ..."
//                << std::endl;
//      try {
//        std::thread([s = std::move(subscriber)]() mutable {
//          try {
//            std::cout << "*** Running in another thread!!!!!" << std::endl;
//            // force artificial delay
//            std::this_thread::sleep_for(std::chrono::milliseconds(100));
//            // now do work
//            std::shared_ptr<Subscriber<Tuple>> sharedSubscriber =
//            std::move(s);
//            std::weak_ptr<Subscriber<Tuple>> wp = sharedSubscriber;
//            sharedSubscriber->onSubscribe(
//                std::make_unique<Source>(std::move(wp)));
//            counter--; // assuming pipeline is synchronous, which it is in
//            this
//            // test
//            std::cout << "Done thread" << std::endl;
//          } catch (std::exception& e) {
//            std::cout << e.what() << std::endl;
//          }
//        }).detach();
//
//      } catch (std::exception& e) {
//        std::cout << e.what() << std::endl;
//      }
//    });
//
//    EXPECT_EQ(0, subscriptionInstanceCount);
//
//    std::cout << "Thread counter: " << counter << std::endl;
//    std::cout << "About to subscribe (1) ..." << std::endl;
//
//    { a->subscribe(std::make_unique<MySubscriber>()); }
//    while (counter > 1) {
//      std::cout << "waiting for completion" << std::endl;
//      std::this_thread::sleep_for(std::chrono::milliseconds(50));
//    }
//    // ensure Subscriber and Subscription are destroyed
//    EXPECT_EQ(0, subscriptionInstanceCount);
//
//    std::cout << "About to subscribe (2) ..." << std::endl;
//
//    { a->subscribe(std::make_unique<MySubscriber>()); }
//    while (counter > 0) {
//      std::cout << "waiting for completion" << std::endl;
//      std::this_thread::sleep_for(std::chrono::milliseconds(50));
//    }
//    // ensure Subscriber and Subscription are destroyed
//    EXPECT_EQ(0, subscriptionInstanceCount);
//
//    std::cout << "Finished ... remaining instances == " << tupleInstanceCount
//              << std::endl;
//  }
//  std::cout << "... after block" << std::endl;
//  EXPECT_EQ(0, tupleInstanceCount);
//  EXPECT_EQ(0, subscriptionInstanceCount);
//  EXPECT_EQ(0, subscriberInstanceCount);
//  std::cout << "-----------------------------" << std::endl;
//}
