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

#include <folly/io/async/EventBase.h>
#include <folly/io/async/EventBaseThread.h>
#include <folly/synchronization/Baton.h>
#include <gtest/gtest.h>
#include <deque>
#include <thread>
#include <type_traits>
#include <vector>
#include "yarpl/Flowable.h"
#include "yarpl/flowable/TestSubscriber.h"
#include "yarpl/test_utils/Mocks.h"

namespace yarpl {
namespace flowable {

namespace {

/// Construct a pipeline with a test subscriber against the supplied
/// flowable.  Return the items that were sent to the subscriber.  If some
/// exception was sent, the exception is thrown.
template <typename T>
std::vector<T> run(
    std::shared_ptr<Flowable<T>> flowable,
    int64_t requestCount = 100) {
  auto subscriber = std::make_shared<TestSubscriber<T>>(requestCount);
  flowable->subscribe(subscriber);
  return std::move(subscriber->values());
}

} // namespace

template <typename Pred>
std::vector<int64_t> filter_run(std::vector<int64_t> in, Pred pred) {
  std::vector<int64_t> ret;
  std::copy_if(in.begin(), in.end(), std::back_inserter(ret), pred);
  return ret;
}

std::vector<int64_t>
filter_range(std::vector<int64_t> in, int64_t startat, int64_t endat) {
  CHECK_LE(startat, endat);
  return filter_run(
      in, [=](int64_t i) { return (i >= startat) && (i < endat); });
}

auto make_flowable_mapper_func() {
  return folly::Function<std::shared_ptr<Flowable<int64_t>>(int)>([](int n) {
    switch (n) {
      case 10:
        return Flowable<>::range(n, 2);
      case 20:
        return Flowable<>::range(n, 3);
      case 30:
        return Flowable<>::range(n, 4);
    }
    return Flowable<>::range(n, 3);
  });
}

// assumes that separate streams of values in separate_streams are entirely
// disjoint
template <typename T>
bool validate_flatmapped_values(
    std::vector<T> flatmapped,
    std::vector<std::deque<T>> separate_streams) {
  for (auto elem : flatmapped) {
    bool found_match = false;
    for (auto& stream : separate_streams) {
      if (stream.size() > 0) {
        if (elem == stream[0]) {
          stream.pop_front();
          found_match = true;
          break;
        }
      }
    }

    EXPECT_TRUE(found_match)
        << "Did not find elem '" << elem << "' in any input streams";
    if (!found_match) {
      return false;
    }
  }

  return true;
}

TEST(FlowableFlatMapTest, AllRequestedTest) {
  auto f = Flowable<>::justN<int>({10, 20, 30})
               ->flatMap(make_flowable_mapper_func());

  std::vector<int64_t> res = run(f);
  EXPECT_EQ(9UL, res.size());
  EXPECT_EQ(filter_range(res, 10, 20), std::vector<int64_t>({10, 11}));
  EXPECT_EQ(filter_range(res, 20, 30), std::vector<int64_t>({20, 21, 22}));
  EXPECT_EQ(filter_range(res, 30, 40), std::vector<int64_t>({30, 31, 32, 33}));
}

TEST(FlowableFlatMapTest, FiniteRequested) {
  auto f = Flowable<>::justN<int>({10, 20, 30})
               ->flatMap(make_flowable_mapper_func());

  auto subscriber = std::make_shared<TestSubscriber<int64_t>>(1);
  f->subscribe(subscriber);

  EXPECT_EQ(1UL, subscriber->values().size());
  EXPECT_TRUE(
      validate_flatmapped_values(subscriber->values(), {{10}, {20}, {30}}));

  subscriber->request(3);
  EXPECT_TRUE(validate_flatmapped_values(
      subscriber->values(), {{10, 11}, {20, 21, 22}, {30, 31, 32, 33}}));
  EXPECT_EQ(subscriber->getValueCount(), 4);
  subscriber->cancel();
  EXPECT_EQ(subscriber->getValueCount(), 4);
}

TEST(FlowableFlatMapTest, MappingLambdaThrowsErrorOnFirstCall) {
  folly::Function<std::shared_ptr<Flowable<int64_t>>(int)> func = [](int n) {
    CHECK_EQ(1, n);
    throw std::runtime_error{"throwing in mapper!"};
    return Flowable<int64_t>::empty();
  };

  auto f = Flowable<>::just<int>(1)->flatMap(std::move(func));

  auto subscriber = std::make_shared<TestSubscriber<int64_t>>(1);
  f->subscribe(subscriber);

  EXPECT_EQ(subscriber->getValueCount(), 0);
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), "throwing in mapper!");
}

TEST(FlowableFlatMapTest, MappedStreamThrows) {
  folly::Function<std::shared_ptr<Flowable<int64_t>>(int)> func = [](int n) {
    CHECK_EQ(1, n);

    // flowable which emits an onNext, then the next iteration, emits an error
    int64_t i = 1;
    return Flowable<int64_t>::create(
        [i](auto& subscriber, int64_t req) mutable {
          CHECK_EQ(1, req);
          if (i > 0) {
            subscriber.onNext(i);
            i--;
          } else {
            subscriber.onError(std::runtime_error{"throwing in stream!"});
          }
        });
  };

  auto f = Flowable<>::just<int>(1)->flatMap(std::move(func));

  auto subscriber = std::make_shared<TestSubscriber<int64_t>>(2);
  f->subscribe(subscriber);

  EXPECT_EQ(subscriber->values(), std::vector<int64_t>({1}));
  EXPECT_TRUE(subscriber->isError());
  EXPECT_EQ(subscriber->getErrorMsg(), "throwing in stream!");
}

struct CBSubscription : yarpl::flowable::Subscription {
  template <typename OnReq, typename OnCancel>
  CBSubscription(OnReq&& r, OnCancel&& c)
      : onRequest(std::move(r)), onCancel(std::move(c)) {}

  void request(int64_t n) override {
    onRequest(n);
  };
  void cancel() override {
    onCancel();
  }

  folly::Function<void(int64_t)> onRequest;
  folly::Function<void(void)> onCancel;
};

struct FlowableEvbPair {
  FlowableEvbPair() = default;
  std::shared_ptr<Flowable<int>> flowable{nullptr};
  folly::EventBaseThread evb{};
};

std::shared_ptr<FlowableEvbPair> make_range_flowable(int start, int end) {
  auto ret = std::make_shared<FlowableEvbPair>();
  ret->evb.start("MRF_Worker");
  ret->flowable = Flowable<>::range(start, end - start)
                      ->map([](int64_t val) { return (int)val; })
                      ->subscribeOn(*ret->evb.getEventBase());
  return ret;
}

TEST(FlowableFlatMapTest, Multithreaded) {
  auto p1 = make_range_flowable(10, 12);
  auto p2 = make_range_flowable(20, 25);

  auto f = Flowable<>::range(0, 2)->flatMap([&](auto i) {
    if (i == 0) {
      return p1->flowable;
    } else {
      return p2->flowable;
    }
  });

  auto sub = std::make_shared<TestSubscriber<int>>(0);
  f->subscribe(sub);

  sub->request(2);
  sub->awaitValueCount(2);
  EXPECT_TRUE(validate_flatmapped_values(sub->values(), {{10, 11}, {20, 21}}));

  sub->cancel();
  p1->evb.stop();
  p2->evb.stop();
}

TEST(FlowableFlatMapTest, MultithreadedLargeAmount) {
  auto p1 = make_range_flowable(10000, 40000);
  auto p2 = make_range_flowable(50000, 80000);

  auto f = Flowable<>::range(0, 2)->flatMap([&](auto i) {
    if (i == 0) {
      return p1->flowable;
    } else {
      return p2->flowable;
    }
  });

  auto sub = std::make_shared<TestSubscriber<int>>();
  sub->dropValues(true);

  f->subscribe(sub);

  sub->awaitTerminalEvent(std::chrono::seconds{5});
  EXPECT_EQ(60000, sub->getValueCount());
  EXPECT_TRUE(sub->isComplete());

  p1->evb.stop();
  p2->evb.stop();
}

TEST(FlowableFlatMapTest, MergeOperator) {
  auto sub = std::make_shared<TestSubscriber<std::string>>(0);

  auto p1 = Flowable<>::justN<std::string>({"foo", "bar"});
  auto p2 = Flowable<>::justN<std::string>({"baz", "quxx"});
  std::shared_ptr<Flowable<std::shared_ptr<Flowable<std::string>>>> p3 =
      Flowable<>::justN<std::shared_ptr<Flowable<std::string>>>({p1, p2});

  std::shared_ptr<Flowable<std::string>> p4 = p3->merge();
  p4->subscribe(sub);

  EXPECT_EQ(0, sub->getValueCount());
  sub->request(1);
  EXPECT_EQ(1, sub->getValueCount());
  EXPECT_EQ(false, sub->isComplete());
  EXPECT_TRUE(validate_flatmapped_values(sub->values(), {{"foo"}, {"baz"}}));

  sub->request(1);
  EXPECT_EQ(2, sub->getValueCount());
  EXPECT_EQ(false, sub->isComplete());
  EXPECT_EQ(false, sub->isError());
  EXPECT_TRUE(validate_flatmapped_values(
      sub->values(), {{"foo", "bar"}, {"baz", "quxx"}}));

  sub->request(1);
  EXPECT_EQ(3, sub->getValueCount());
  EXPECT_EQ(false, sub->isComplete());
  EXPECT_EQ(false, sub->isError());
  EXPECT_TRUE(validate_flatmapped_values(
      sub->values(), {{"foo", "bar"}, {"baz", "quxx"}}));

  sub->request(1);
  EXPECT_EQ(4, sub->getValueCount());
  EXPECT_EQ(true, sub->isComplete());
  EXPECT_EQ(false, sub->isError());
  EXPECT_TRUE(validate_flatmapped_values(
      sub->values(), {{"foo", "bar"}, {"baz", "quxx"}}));
}

} // namespace flowable
} // namespace yarpl
