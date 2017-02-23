#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <yarpl/Flowable.h>

using namespace yarpl::flowable;

/*
 * Verbose usage of interfaces with inline class implementations.
 *
 * - Demonstrating subscribe, onSubscribe, request, onNext, cancel.
 * - Shows use of unique_ptr and std::move to pass around the Subscriber and
 * Subscription.
 */
TEST(Flowable, SubscribeRequestAndCancel) {

  // Subscription that emits integers forever as long as requested
  class InfiniteIntegersSource : public Subscription {
    std::unique_ptr<Subscriber<int>> subscriber;
    std::atomic_bool isCancelled{false};

  public:
    InfiniteIntegersSource(std::unique_ptr<Subscriber<int>> subscriber)
        : subscriber(std::move(subscriber)) {}

    void cancel() override { isCancelled = true; }
    void request(uint64_t n) override {
      for (u_long i = 0; i < n; i++) {
        if (isCancelled) {
          return;
        }
        subscriber->onNext(i);
      }
    }
  };

  // Subscriber that requests 10 items, then cancels after receiving 6
  class MySubscriber : public Subscriber<int> {
    std::unique_ptr<Subscription> subscription;

  public:
    void onNext(const int &value) override {
      std::cout << "received " << value << std::endl;
      if (value == 6) {
        subscription->cancel();
      }
    }
    void onError(const std::exception &e) override {}
    void onComplete() override {}
    void onSubscribe(std::unique_ptr<Subscription> s) override {
      subscription = std::move(s);
      subscription->request((u_long)10);
    }
  };

  // create Flowable around InfiniteIntegersSource
  auto a =
      Flowable<int>::create([](std::unique_ptr<Subscriber<int>> subscriber) {
        /*
         * This usage works, and looks to be supported by rule 14 of:
         * http://en.cppreference.com/w/cpp/language/eval_order
         *
         * This rule is added for C++17, but talking with Lee Howes on the C++
         * committee
         * suggests this is codifying behavior that already is supported by
         * compilers.
         *
         * One remaining concern though is that this following line is basically
         * the only
         * way to code it, as the std:move could not be done on a line
         * preceeding onSubscribe.
         *
         * Another option is to make the Subscription and move Subscriber into
         * it, then
         * have an accessor on Subscription to allow something like:
         *
         * subscription->getSubscriber()->onSubscribe(subscription).
         *
         * But that's awkward too.
         *
         * Another option is to make the Subscription call onSubscribe once
         * the Subscriber is moved into it.
         *
         * Or we just accept that this line is how it's done. In practice
         * very few engineers will ever need to write this code because
         * implementations
         * will typically provide factory methods anyways, since it's hard to
         * create
         * proper Flowable/Publishers without them.
         */
        subscriber->onSubscribe(
            std::make_unique<InfiniteIntegersSource>(std::move(subscriber)));
      });

  a->subscribe(std::make_unique<MySubscriber>());
  std::cout << "-----------------------------" << std::endl;
}

TEST(Flowable, OnError) {

  // Subscription that fails
  class Source : public Subscription {
    std::unique_ptr<Subscriber<int>> subscriber;
    std::atomic_bool isCancelled{false};

  public:
    Source(std::unique_ptr<Subscriber<int>> subscriber)
        : subscriber(std::move(subscriber)) {}

    void cancel() override { isCancelled = true; }
    void request(uint64_t n) override {
      try {
        throw std::runtime_error("something broke!");
      } catch (const std::exception &e) {
        subscriber->onError(e);
      }
    }
  };

  // create Flowable around Source
  auto a = Flowable<int>::create([](
      std::unique_ptr<Subscriber<int>> subscriber) {
    subscriber->onSubscribe(std::make_unique<Source>(std::move(subscriber)));
  });

  std::string errorMessage("DEFAULT->No Error Message");
  a->subscribe(
      Subscriber<int>::create([](int value) { /* do nothing */ },
                              [&errorMessage](const std::exception &e) {
                                errorMessage = std::string(e.what());
                              }));

  EXPECT_EQ("something broke!", errorMessage);
  std::cout << "-----------------------------" << std::endl;
}

/**
 * Assert that all items passed through the Flowable get destroyed
 */
TEST(Flowable, ItemsCollectedSynchronously) {

  static std::atomic<int> instanceCount;

  struct Tuple {
    const int a;
    const int b;

    Tuple(const int a, const int b) : a(a), b(b) {
      std::cout << "Tuple created!!" << std::endl;
      instanceCount++;
    }
    Tuple(const Tuple &t) : a(t.a), b(t.b) {
      std::cout << "Tuple copy constructed!!" << std::endl;
      instanceCount++;
    }
    ~Tuple() {
      std::cout << "Tuple destroyed!!" << std::endl;
      instanceCount--;
    }
  };

  class Source : public Subscription {
    std::unique_ptr<Subscriber<Tuple>> subscriber;
    std::atomic_bool isCancelled{false};

  public:
    Source(std::unique_ptr<Subscriber<Tuple>> subscriber)
        : subscriber(std::move(subscriber)) {}

    void cancel() override { isCancelled = true; }
    void request(uint64_t n) override {
      // ignoring n for tests ... DO NOT DO THIS FOR REAL
      subscriber->onNext(Tuple{1, 2});
      subscriber->onNext(Tuple{2, 3});
      subscriber->onNext(Tuple{3, 4});
      subscriber->onComplete();
    }
  };

  // create Flowable around Source
  auto a = Flowable<Tuple>::create([](
      std::unique_ptr<Subscriber<Tuple>> subscriber) {
    subscriber->onSubscribe(std::make_unique<Source>(std::move(subscriber)));
  });

  a->subscribe(Subscriber<Tuple>::create([](const Tuple &value) {
    std::cout << "received value " << value.a << std::endl;
  }));

  std::cout << "Finished ... remaining instances == " << instanceCount
            << std::endl;

  EXPECT_EQ(0, instanceCount);
  std::cout << "-----------------------------" << std::endl;
}

/*
 * Assert that all items passed through the Flowable get
 * copied and destroyed correctly over async boundaries.
 *
 * This is simulating "async" by having an Observer store the items
 * in a Vector which could then be consumed on another thread.
 */
TEST(Flowable, ItemsCollectedAsynchronously) {

  static std::atomic<int> createdCount;
  static std::atomic<int> destroyedCount;

  struct Tuple {
    const int a;
    const int b;

    Tuple(const int a, const int b) : a(a), b(b) {
      std::cout << "Tuple " << a << " created!!" << std::endl;
      createdCount++;
    }
    Tuple(const Tuple &t) : a(t.a), b(t.b) {
      std::cout << "Tuple " << a << " copy constructed!!" << std::endl;
      createdCount++;
    }
    ~Tuple() {
      std::cout << "Tuple " << a << " destroyed!!" << std::endl;
      destroyedCount++;
    }
  };

  // scope this so we can check destruction of Vector after this block
  {

    // Subscription that fails
    class Source : public Subscription {
      std::unique_ptr<Subscriber<Tuple>> subscriber;
      std::atomic_bool isCancelled{false};

    public:
      Source(std::unique_ptr<Subscriber<Tuple>> subscriber)
          : subscriber(std::move(subscriber)) {}

      void cancel() override { isCancelled = true; }
      void request(uint64_t n) override {
        // ignoring n for tests ... DO NOT DO THIS FOR REAL
        std::cout << "-----------------------------" << std::endl;
        subscriber->onNext(Tuple{1, 2});
        std::cout << "-----------------------------" << std::endl;
        subscriber->onNext(Tuple{2, 3});
        std::cout << "-----------------------------" << std::endl;
        subscriber->onNext(Tuple{3, 4});
        std::cout << "-----------------------------" << std::endl;
        subscriber->onComplete();
      }
    };

    // create Flowable around Source
    auto a = Flowable<Tuple>::create([](
        std::unique_ptr<Subscriber<Tuple>> subscriber) {
      subscriber->onSubscribe(std::make_unique<Source>(std::move(subscriber)));
    });

    std::vector<Tuple> v;
    v.reserve(10); // otherwise it resizes and copies on each push_back
    a->subscribe(Subscriber<Tuple>::create([&v](const Tuple &value) {
      std::cout << "received value " << value.a << std::endl;
      // copy into vector
      v.push_back(value);
      std::cout << "done pushing into vector" << std::endl;
    }));

    // expect that 3 instances were originally created, then 3 more when copying
    EXPECT_EQ(6, createdCount);
    // expect that 3 instances still exist in the vector, so only 3 destroyed so
    // far
    EXPECT_EQ(3, destroyedCount);

    std::cout << "Leaving block now so Vector should release Tuples..."
              << std::endl;
  }

  EXPECT_EQ(0, (createdCount - destroyedCount));
  std::cout << "-----------------------------" << std::endl;
}

/**
 * Assert that memory is not retained when a single Flowable
 * is subscribed to multiple times.
 */
TEST(Flowable, TestRetainOnStaticAsyncFlowableWithMultipleSubscribers) {

  static std::atomic<int> tupleInstanceCount;
  static std::atomic<int> subscriptionInstanceCount;
  static std::atomic<int> subscriberInstanceCount;

  struct Tuple {
    const int a;
    const int b;

    Tuple(const int a, const int b) : a(a), b(b) {
      std::cout << "   Tuple created!!" << std::endl;
      tupleInstanceCount++;
    }
    Tuple(const Tuple &t) : a(t.a), b(t.b) {
      std::cout << "   Tuple copy constructed!!" << std::endl;
      tupleInstanceCount++;
    }
    ~Tuple() {
      std::cout << "   Tuple destroyed!!" << std::endl;
      tupleInstanceCount--;
    }
  };

  class MySubscriber : public Subscriber<Tuple> {
    std::unique_ptr<Subscription> subscription;

  public:
    MySubscriber() {
      std::cout << "   Subscriber created!!" << std::endl;
      subscriberInstanceCount++;
    }
    ~MySubscriber() {
      std::cout << "   Subscriber destroyed!!" << std::endl;
      subscriberInstanceCount--;
    }

    void onNext(const Tuple &value) override {
      std::cout << "   Subscriber received value " << value.a << " on thread "
                << std::this_thread::get_id() << std::endl;
    }
    void onError(const std::exception &e) override {}
    void onComplete() override {}
    void onSubscribe(std::unique_ptr<Subscription> s) override {
      subscription = std::move(s);
      subscription->request((u_long)10);
    }
  };

  class Source : public Subscription {
    std::weak_ptr<Subscriber<Tuple>> subscriber;
    std::atomic_bool isCancelled{false};

  public:
    Source(std::weak_ptr<Subscriber<Tuple>> subscriber)
        : subscriber(std::move(subscriber)) {
      std::cout << "   Subscription created!!" << std::endl;
      subscriptionInstanceCount++;
    }
    ~Source() {
      std::cout << "   Subscription destroyed!!" << std::endl;
      subscriptionInstanceCount--;
    }

    void cancel() override { isCancelled = true; }
    void request(uint64_t n) override {
      // ignoring n for tests ... DO NOT DO THIS FOR REAL
      auto ss = subscriber.lock();
      if (ss) {
        ss->onNext(Tuple{1, 2});
        ss->onNext(Tuple{2, 3});
        ss->onNext(Tuple{3, 4});
        ss->onComplete();
      } else {
        std::cout << "!!!!!!!!!!! Subscriber already destroyed when REQUEST_N "
                     "received."
                  << std::endl;
      }
    }
  };

  {
    static std::atomic_int counter{2};
    // create Flowable around Source
    auto a = Flowable<Tuple>::create([](
        std::unique_ptr<Subscriber<Tuple>> subscriber) {
      std::cout << "Flowable subscribed to ... starting new thread ..."
                << std::endl;
      try {
        std::thread([s = std::move(subscriber)]() mutable {
          try {
            std::cout << "*** Running in another thread!!!!!" << std::endl;
            // force artificial delay
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // now do work
            std::shared_ptr<Subscriber<Tuple>> sharedSubscriber = std::move(s);
            std::weak_ptr<Subscriber<Tuple>> wp = sharedSubscriber;
            sharedSubscriber->onSubscribe(
                std::make_unique<Source>(std::move(wp)));
            counter--; // assuming pipeline is synchronous, which it is in this
                       // test
            std::cout << "Done thread" << std::endl;
          } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
          }
        }).detach();

      } catch (std::exception &e) {
        std::cout << e.what() << std::endl;
      }
    });

    EXPECT_EQ(0, subscriptionInstanceCount);

    std::cout << "Thread counter: " << counter << std::endl;
    std::cout << "About to subscribe (1) ..." << std::endl;

    { a->subscribe(std::make_unique<MySubscriber>()); }
    while (counter > 1) {
      std::cout << "waiting for completion" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // ensure Subscriber and Subscription are destroyed
    EXPECT_EQ(0, subscriptionInstanceCount);

    std::cout << "About to subscribe (2) ..." << std::endl;

    { a->subscribe(std::make_unique<MySubscriber>()); }
    while (counter > 0) {
      std::cout << "waiting for completion" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // ensure Subscriber and Subscription are destroyed
    EXPECT_EQ(0, subscriptionInstanceCount);

    std::cout << "Finished ... remaining instances == " << tupleInstanceCount
              << std::endl;
  }
  std::cout << "... after block" << std::endl;
  EXPECT_EQ(0, tupleInstanceCount);
  EXPECT_EQ(0, subscriptionInstanceCount);
  EXPECT_EQ(0, subscriberInstanceCount);
  std::cout << "-----------------------------" << std::endl;
}