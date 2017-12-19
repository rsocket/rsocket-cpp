// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/synchronization/Baton.h>
#include <gtest/gtest.h>
#include <thread>
#include <type_traits>
#include <vector>

#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"
#include "yarpl/test_utils/Mocks.h"

namespace yarpl {
namespace flowable {
namespace {

/*
 * Used in place of TestSubscriber where we have move-only types.
 */
template <typename T>
class CollectingSubscriber : public BaseSubscriber<T> {
 public:
  explicit CollectingSubscriber(int64_t requestCount = 100)
      : requestCount_(requestCount) {}

  void onSubscribeImpl() override {
    this->request(requestCount_);
  }

  void onNextImpl(T next) override {
    values_.push_back(std::move(next));
  }

  void onCompleteImpl() override {
    complete_ = true;
  }

  void onErrorImpl(folly::exception_wrapper ex) override {
    error_ = true;
    errorMsg_ = ex.get_exception()->what();
  }

  std::vector<T>& values() {
    return values_;
  }

  bool isComplete() const {
    return complete_;
  }

  bool isError() const {
    return error_;
  }

  const std::string& errorMsg() const {
    return errorMsg_;
  }

  void cancelSubscription() {
    this->cancel();
  }

 private:
  std::vector<T> values_;
  std::string errorMsg_;
  bool complete_{false};
  bool error_{false};
  int64_t requestCount_;
};

/// Construct a pipeline with a test subscriber against the supplied
/// flowable.  Return the items that were sent to the subscriber.  If some
/// exception was sent, the exception is thrown.
template <typename T>
std::vector<T> run(
    Reference<Flowable<T>> flowable,
    int64_t requestCount = 100) {
  auto subscriber = make_ref<TestSubscriber<T>>(requestCount);
  flowable->subscribe(subscriber);
  return std::move(subscriber->values());
}

} // namespace

TEST(FlowableTest, SingleFlowable) {
  auto flowable = Flowables::just(10);
  flowable.reset();
}

TEST(FlowableTest, SingleMovableFlowable) {
  auto value = std::make_unique<int>(123456);

  auto flowable = Flowables::justOnce(std::move(value));
  EXPECT_EQ(1, flowable.use_count());

  size_t received = 0;
  auto subscriber =
      Subscribers::create<std::unique_ptr<int>>([&](std::unique_ptr<int> p) {
        EXPECT_EQ(*p, 123456);
        received++;
      });

  flowable->subscribe(std::move(subscriber));
  EXPECT_EQ(received, 1u);
}

TEST(FlowableTest, JustFlowable) {
  EXPECT_EQ(run(Flowables::just(22)), std::vector<int>{22});
  EXPECT_EQ(
      run(Flowables::justN({12, 34, 56, 98})),
      std::vector<int>({12, 34, 56, 98}));
  EXPECT_EQ(
      run(Flowables::justN({"ab", "pq", "yz"})),
      std::vector<const char*>({"ab", "pq", "yz"}));
}

TEST(FlowableTest, JustIncomplete) {
  auto flowable = Flowables::justN<std::string>({"a", "b", "c"})->take(2);
  EXPECT_EQ(run(std::move(flowable)), std::vector<std::string>({"a", "b"}));

  flowable = Flowables::justN<std::string>({"a", "b", "c"})->take(2)->take(1);
  EXPECT_EQ(run(std::move(flowable)), std::vector<std::string>({"a"}));
  flowable.reset();

  flowable = Flowables::justN<std::string>(
                 {"a", "b", "c", "d", "e", "f", "g", "h", "i"})
                 ->map([](std::string s) {
                   s[0] = ::toupper(s[0]);
                   return s;
                 })
                 ->take(5);

  EXPECT_EQ(
      run(std::move(flowable)),
      std::vector<std::string>({"A", "B", "C", "D", "E"}));
  flowable.reset();
}

TEST(FlowableTest, MapWithException) {
  auto flowable = Flowables::justN<int>({1, 2, 3, 4})->map([](int n) {
    if (n > 2) {
      throw std::runtime_error{"Too big!"};
    }
    return n;
  });

  auto subscriber = yarpl::make_ref<TestSubscriber<int>>();
  flowable->subscribe(subscriber);

  EXPECT_EQ(subscriber->values(), std::vector<int>({1, 2}));
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), "Too big!");
}

TEST(FlowableTest, Range) {
  EXPECT_EQ(
      run(Flowables::range(10, 5)), std::vector<int64_t>({10, 11, 12, 13, 14}));
}

TEST(FlowableTest, RangeWithMap) {
  auto flowable = Flowables::range(1, 3)
                      ->map([](int64_t v) { return v * v; })
                      ->map([](int64_t v) { return v * v; })
                      ->map([](int64_t v) { return std::to_string(v); });
  EXPECT_EQ(
      run(std::move(flowable)), std::vector<std::string>({"1", "16", "81"}));
}

TEST(FlowableTest, RangeWithReduceMoreItems) {
  auto flowable = Flowables::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<int64_t>({45}));
}

TEST(FlowableTest, RangeWithReduceByMultiplication) {
  auto flowable = Flowables::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc * v; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<int64_t>({0}));

  flowable = Flowables::range(1, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc * v; });
  EXPECT_EQ(
      run(std::move(flowable)),
      std::vector<int64_t>({2 * 3 * 4 * 5 * 6 * 7 * 8 * 9 * 10}));
}

TEST(FlowableTest, RangeWithReduceLessItems) {
  auto flowable = Flowables::range(0, 10)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  // Even if we ask for 1 item only, it will reduce all the items
  EXPECT_EQ(run(std::move(flowable), 5), std::vector<int64_t>({45}));
}

TEST(FlowableTest, RangeWithReduceOneItem) {
  auto flowable = Flowables::range(5, 1)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<int64_t>({5}));
}

TEST(FlowableTest, RangeWithReduceNoItem) {
  auto flowable = Flowables::range(0, 0)->reduce(
      [](int64_t acc, int64_t v) { return acc + v; });
  auto subscriber = make_ref<TestSubscriber<int64_t>>(100);
  flowable->subscribe(subscriber);

  EXPECT_TRUE(subscriber->isComplete());
  EXPECT_EQ(subscriber->values(), std::vector<int64_t>({}));
}

TEST(FlowableTest, RangeWithFilterAndReduce) {
  auto flowable = Flowables::range(0, 10)
                      ->filter([](int64_t v) { return v % 2 != 0; })
                      ->reduce([](int64_t acc, int64_t v) { return acc + v; });
  EXPECT_EQ(
      run(std::move(flowable)), std::vector<int64_t>({1 + 3 + 5 + 7 + 9}));
}

TEST(FlowableTest, RangeWithReduceToBiggerType) {
  auto flowable = Flowables::range(5, 1)
                      ->map([](int64_t v) { return (char)(v + 10); })
                      ->reduce([](int64_t acc, char v) { return acc + v; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<int64_t>({15}));
}

TEST(FlowableTest, StringReduce) {
  auto flowable =
      Flowables::justN<std::string>(
          {"a", "b", "c", "d", "e", "f", "g", "h", "i"})
          ->reduce([](std::string acc, std::string v) { return acc + v; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<std::string>({"abcdefghi"}));
}

TEST(FlowableTest, RangeWithFilterRequestMoreItems) {
  auto flowable =
      Flowables::range(0, 10)->filter([](int64_t v) { return v % 2 != 0; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<int64_t>({1, 3, 5, 7, 9}));
}

TEST(FlowableTest, RangeWithFilterRequestLessItems) {
  auto flowable =
      Flowables::range(0, 10)->filter([](int64_t v) { return v % 2 != 0; });
  EXPECT_EQ(run(std::move(flowable), 5), std::vector<int64_t>({1, 3, 5, 7, 9}));
}

TEST(FlowableTest, RangeWithFilterAndMap) {
  auto flowable = Flowables::range(0, 10)
                      ->filter([](int64_t v) { return v % 2 != 0; })
                      ->map([](int64_t v) { return v + 10; });
  EXPECT_EQ(
      run(std::move(flowable)), std::vector<int64_t>({11, 13, 15, 17, 19}));
}

TEST(FlowableTest, RangeWithMapAndFilter) {
  auto flowable = Flowables::range(0, 10)
                      ->map([](int64_t v) { return (char)(v + 10); })
                      ->filter([](char v) { return v % 2 != 0; });
  EXPECT_EQ(run(std::move(flowable)), std::vector<char>({11, 13, 15, 17, 19}));
}

TEST(FlowableTest, SimpleTake) {
  EXPECT_EQ(
      run(Flowables::range(0, 100)->take(3)), std::vector<int64_t>({0, 1, 2}));
  EXPECT_EQ(
      run(Flowables::range(10, 5)), std::vector<int64_t>({10, 11, 12, 13, 14}));
}

TEST(FlowableTest, SimpleSkip) {
  EXPECT_EQ(
      run(Flowables::range(0, 10)->skip(8)), std::vector<int64_t>({8, 9}));
}

TEST(FlowableTest, OverflowSkip) {
  EXPECT_EQ(run(Flowables::range(0, 10)->skip(12)), std::vector<int64_t>({}));
}

TEST(FlowableTest, SkipPartial) {
  auto subscriber = make_ref<TestSubscriber<int64_t>>(2);
  auto flowable = Flowables::range(0, 10)->skip(5);
  flowable->subscribe(subscriber);

  EXPECT_EQ(subscriber->values(), std::vector<int64_t>({5, 6}));
  subscriber->cancel();
}

TEST(FlowableTest, IgnoreElements) {
  auto flowable = Flowables::range(0, 100)->ignoreElements()->map(
      [](int64_t v) { return v * v; });
  EXPECT_EQ(run(flowable), std::vector<int64_t>({}));
}

TEST(FlowableTest, IgnoreElementsPartial) {
  auto subscriber = make_ref<TestSubscriber<int64_t>>(5);
  auto flowable = Flowables::range(0, 10)->ignoreElements();
  flowable->subscribe(subscriber);

  EXPECT_EQ(subscriber->values(), std::vector<int64_t>({}));
  EXPECT_FALSE(subscriber->isComplete());
  EXPECT_FALSE(subscriber->isError());

  subscriber->cancel();
}

TEST(FlowableTest, IgnoreElementsError) {
  constexpr auto kMsg = "Failure";

  auto subscriber = make_ref<TestSubscriber<int>>();
  auto flowable = Flowables::error<int>(std::runtime_error(kMsg));
  flowable->subscribe(subscriber);

  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), kMsg);
}

TEST(FlowableTest, FlowableError) {
  constexpr auto kMsg = "something broke!";

  auto flowable = Flowables::error<int>(std::runtime_error(kMsg));
  auto subscriber = make_ref<TestSubscriber<int>>();
  flowable->subscribe(subscriber);

  EXPECT_FALSE(subscriber->isComplete());
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), kMsg);
}

TEST(FlowableTest, FlowableErrorPtr) {
  constexpr auto kMsg = "something broke!";

  auto flowable = Flowables::error<int>(std::runtime_error(kMsg));
  auto subscriber = make_ref<TestSubscriber<int>>();
  flowable->subscribe(subscriber);

  EXPECT_FALSE(subscriber->isComplete());
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), kMsg);
}

TEST(FlowableTest, FlowableEmpty) {
  auto flowable = Flowables::empty<int>();
  auto subscriber = make_ref<TestSubscriber<int>>();
  flowable->subscribe(subscriber);

  EXPECT_TRUE(subscriber->isComplete());
  EXPECT_FALSE(subscriber->isError());
}

TEST(FlowableTest, FlowableFromGenerator) {
  auto flowable = Flowables::fromGenerator<std::unique_ptr<int>>(
      [] { return std::unique_ptr<int>(); });

  auto subscriber = make_ref<CollectingSubscriber<std::unique_ptr<int>>>(10);
  flowable->subscribe(subscriber);

  EXPECT_FALSE(subscriber->isComplete());
  EXPECT_FALSE(subscriber->isError());
  EXPECT_EQ(std::size_t{10}, subscriber->values().size());

  subscriber->cancelSubscription();
}

TEST(FlowableTest, FlowableFromGeneratorException) {
  constexpr auto errorMsg = "error from generator";
  int count = 5;
  auto flowable = Flowables::fromGenerator<std::unique_ptr<int>>([&] {
    while (count--) {
      return std::unique_ptr<int>();
    }
    throw std::runtime_error(errorMsg);
  });

  auto subscriber = make_ref<CollectingSubscriber<std::unique_ptr<int>>>(10);
  flowable->subscribe(subscriber);

  EXPECT_FALSE(subscriber->isComplete());
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->errorMsg(), errorMsg);
  EXPECT_EQ(std::size_t{5}, subscriber->values().size());
}

TEST(FlowableTest, SubscribersComplete) {
  auto flowable = Flowables::empty<int>();
  auto subscriber = Subscribers::create<int>(
      [](int) { FAIL(); }, [](folly::exception_wrapper) { FAIL(); }, [&] {});
  flowable->subscribe(std::move(subscriber));
}

TEST(FlowableTest, SubscribersError) {
  auto flowable = Flowables::error<int>(std::runtime_error("Whoops"));
  auto subscriber = Subscribers::create<int>(
      [](int) { FAIL(); }, [&](folly::exception_wrapper) {}, [] { FAIL(); });
  flowable->subscribe(std::move(subscriber));
}

TEST(FlowableTest, FlowableCompleteInTheMiddle) {
  auto flowable =
      Flowable<int>::create(
          [](Reference<Subscriber<int>> subscriber, int64_t requested) {
            EXPECT_GT(requested, 1);
            subscriber->onNext(123);
            subscriber->onComplete();
            return std::make_tuple(int64_t(1), true);
          })
          ->map([](int v) { return std::to_string(v); });

  auto subscriber = make_ref<TestSubscriber<std::string>>(10);
  flowable->subscribe(subscriber);

  EXPECT_TRUE(subscriber->isComplete());
  EXPECT_FALSE(subscriber->isError());
  EXPECT_EQ(std::size_t{1}, subscriber->values().size());
}

class RangeCheckingSubscriber : public BaseSubscriber<int32_t> {
 public:
  explicit RangeCheckingSubscriber(int32_t total, folly::Baton<>& b)
      : total_(total), onComplete_(b) {}

  void onSubscribeImpl() override {
    this->request(total_);
  }

  void onNextImpl(int32_t val) override {
    EXPECT_EQ(val, current_);
    current_++;
  }

  void onErrorImpl(folly::exception_wrapper) override {
    FAIL() << "shouldn't call onError";
  }

  void onCompleteImpl() override {
    EXPECT_EQ(total_, current_);
    onComplete_.post();
  }

 private:
  int32_t current_{0};
  int32_t total_;
  folly::Baton<>& onComplete_;
};

namespace {
// workaround for gcc-4.9
auto const expect_count = 10000;
TEST(FlowableTest, FlowableFromDifferentThreads) {
  auto flowable = Flowable<int32_t>::create([&](auto subscriber, int64_t req) {
    EXPECT_EQ(req, expect_count);
    auto t1 = std::thread([&] {
      for (int32_t i = 0; i < req; i++) {
        subscriber->onNext(i);
      }

      subscriber->onComplete();
    });
    t1.join();
    return std::make_tuple(req, true);
  });

  auto t2 = std::thread([&] {
    folly::Baton<> on_flowable_complete;
    flowable->subscribe(yarpl::make_ref<RangeCheckingSubscriber>(
        expect_count, on_flowable_complete));
    on_flowable_complete.timed_wait(std::chrono::milliseconds(100));
  });

  t2.join();
}
} // namespace

class ErrorRangeCheckingSubscriber : public BaseSubscriber<int32_t> {
 public:
  explicit ErrorRangeCheckingSubscriber(
      int32_t expect,
      int32_t request,
      folly::Baton<>& b,
      folly::exception_wrapper expected_err)
      : expect_(expect),
        request_(request),
        onError_(b),
        expectedErr_(expected_err) {}

  void onSubscribeImpl() override {
    this->request(request_);
  }

  void onNextImpl(int32_t val) override {
    EXPECT_EQ(val, current_);
    current_++;
  }

  void onErrorImpl(folly::exception_wrapper err) override {
    EXPECT_EQ(expect_, current_);
    EXPECT_TRUE(err);
    EXPECT_EQ(
        err.get_exception()->what(), expectedErr_.get_exception()->what());
    onError_.post();
  }

  void onCompleteImpl() override {
    FAIL() << "shouldn't ever onComplete";
  }

 private:
  int32_t expect_;
  int32_t request_;
  folly::Baton<>& onError_;
  folly::exception_wrapper expectedErr_;
  int32_t current_{0};
};

namespace {
// workaround for gcc-4.9
auto const request = 10000;
auto const expect = 5000;
auto const the_ex = folly::make_exception_wrapper<std::runtime_error>("wat");

TEST(FlowableTest, FlowableFromDifferentThreadsWithError) {
  auto flowable = Flowable<int32_t>::create([=](auto subscriber, int64_t req) {
    EXPECT_EQ(req, request);
    EXPECT_LT(expect, request);

    auto t1 = std::thread([&] {
      for (int32_t i = 0; i < expect; i++) {
        subscriber->onNext(i);
      }

      subscriber->onError(the_ex);
    });
    t1.join();
    return std::make_tuple<int64_t, bool>(expect, true);
  });

  auto t2 = std::thread([&] {
    folly::Baton<> on_flowable_error;
    flowable->subscribe(yarpl::make_ref<ErrorRangeCheckingSubscriber>(
        expect, request, on_flowable_error, the_ex));
    on_flowable_error.timed_wait(std::chrono::milliseconds(100));
  });

  t2.join();
}
} // namespace

TEST(FlowableTest, SubscribeMultipleTimes) {
  using namespace ::testing;
  using StrictMockSubscriber =
      testing::StrictMock<yarpl::mocks::MockSubscriber<int64_t>>;
  auto f = Flowable<int64_t>::create([](auto subscriber, int64_t req) {
    for (int64_t i = 0; i < req; i++) {
      subscriber->onNext(i);
    }

    subscriber->onComplete();
    return std::make_tuple(req, true);
  });

  auto setup_mock = [](auto request_num, auto& resps) {
    auto mock = make_ref<StrictMockSubscriber>(request_num);

    Sequence seq;
    EXPECT_CALL(*mock, onSubscribe_(_)).InSequence(seq);
    EXPECT_CALL(*mock, onNext_(_))
        .InSequence(seq)
        .WillRepeatedly(
            Invoke([&resps](int64_t value) { resps.push_back(value); }));
    EXPECT_CALL(*mock, onComplete_()).InSequence(seq);
    return mock;
  };

  std::vector<std::vector<int64_t>> results{5};
  auto mock1 = setup_mock(5, results[0]);
  auto mock2 = setup_mock(5, results[1]);
  auto mock3 = setup_mock(5, results[2]);
  auto mock4 = setup_mock(5, results[3]);
  auto mock5 = setup_mock(5, results[4]);

  // map on the same flowable twice
  auto stream1 = f->map([](auto i) { return i + 1; });
  auto stream2 = f->map([](auto i) { return i * 2; });
  auto stream3 = stream2->skip(2); // skip operator chained after a map operator
  auto stream4 = stream1->take(3); // take operator chained after a map operator
  auto stream5 = stream1; // test subscribing to exact same flowable twice

  stream1->subscribe(mock1);
  stream2->subscribe(mock2);
  stream3->subscribe(mock3);
  stream4->subscribe(mock4);
  stream5->subscribe(mock5);

  EXPECT_EQ(results[0], std::vector<int64_t>({1, 2, 3, 4, 5}));
  EXPECT_EQ(results[1], std::vector<int64_t>({0, 2, 4, 6, 8}));
  EXPECT_EQ(results[2], std::vector<int64_t>({4, 6, 8, 10, 12}));
  EXPECT_EQ(results[3], std::vector<int64_t>({1, 2, 3}));
  EXPECT_EQ(results[4], std::vector<int64_t>({1, 2, 3, 4, 5}));
}

/* following test should probably behave like:
 *
TEST(FlowableTest, ConsumerThrows_OnNext) {
  auto range = Flowables::range(1, 10);

  EXPECT_THROWS({
    range->subscribe(
        // onNext
        [](auto) { throw std::runtime_error("throw at consumption"); },
        // onError
        [](auto) { FAIL(); },
        // onComplete
        []() { FAIL(); });
  });
}
*/
TEST(FlowableTest, ConsumerThrows_OnNext) {
  bool onErrorIsCalled{false};

  Flowables::range(1, 10)->subscribe(
      [](auto) { throw std::runtime_error("throw at consumption"); },
      [&onErrorIsCalled](auto ex) { onErrorIsCalled = true; },
      []() { FAIL() << "onError should have been called"; });

  EXPECT_TRUE(onErrorIsCalled);
}

TEST(FlowableTest, ConsumerThrows_OnError) {
  try {
    Flowables::range(1, 10)->subscribe(
        [](auto) { throw std::runtime_error("throw at consumption"); },
        [](auto) { throw std::runtime_error("throw at onError"); },
        []() { FAIL() << "onError should have been called"; });
  } catch (const std::runtime_error& exn) {
    FAIL() << "Error thrown in onError should have been caught.";
  } catch (...) {
    LOG(INFO) << "The app crashes in DEBUG mode to inform the implementor.";
  }
}

TEST(FlowableTest, ConsumerThrows_OnComplete) {
  try {
    Flowables::range(1, 10)->subscribe(
        [](auto) {},
        [](auto) { FAIL() << "onComplete should have been called"; },
        []() { throw std::runtime_error("throw at onComplete"); });
  } catch (const std::runtime_error&) {
    FAIL() << "Error thrown in onComplete should have been caught.";
  } catch (...) {
    LOG(INFO) << "The app crashes in DEBUG mode to inform the implementor.";
  }
}

} // namespace flowable
} // namespace yarpl
