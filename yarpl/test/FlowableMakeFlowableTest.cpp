// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/Flowable.h"
#include <gtest/gtest.h>
#include <type_traits>
#include <vector>
#include "yarpl/flowable/TestSubscriber.h"
#include "yarpl/test_utils/Mocks.h"

namespace yarpl {
namespace flowable {

namespace {

// until proper CallbackSubscription lands
template <typename OnReq, typename OnCancel>
struct TmpCallbackSubscription : Subscription {
  OnReq req_;
  OnCancel cancel_;

  TmpCallbackSubscription(OnReq r, OnCancel c)
      : req_(std::move(r)), cancel_(std::move(c)) {}

  void cancel() override {
    cancel_();
  }

  void request(int64_t req) override {
    req_(req);
  }
};

template <typename OnReq, typename OnCancel>
auto makeCallbackSub(OnReq r, OnCancel c) {
  return std::make_shared<TmpCallbackSubscription<OnReq, OnCancel>>(
      std::move(r), std::move(c));
}

template <typename OnReq>
auto makeCallbackSub(OnReq r) {
  return makeCallbackSub(r, []() {});
}

std::shared_ptr<Flowable<int>> makeRange(int max = 10000) {
  struct State {
    int i;
    int max;
    State(int i, int max) : i{i}, max{max} {}
  };

  return Flowable<int>::makeFlowable([max](auto subscriber) {
    auto state = std::make_shared<State>(0, max);
    return makeCallbackSub([=](int64_t req) {
      while (req--) {
        subscriber->onNext(state->i++);
        if (state->i == state->max) {
          subscriber->onComplete();
          break;
        }
      }
    });
  });
}

} // namespace

TEST(MakeFlowableTest, TestTakeLimited) {
  auto flowable = makeRange();
  auto ts = std::make_shared<TestSubscriber<int>>(5);
  flowable->subscribe(ts);
  ts->awaitValueCount(5);
  EXPECT_EQ(ts->values().size(), 5UL);
  EXPECT_EQ(ts->values(), (std::vector<int>{0, 1, 2, 3, 4}));
  EXPECT_TRUE(!ts->isComplete()); // subscriber only ran out of credits, range
                                  // hasn't ran out though
}

TEST(MakeFlowableTest, TestTakeOpZero) {
  auto flowable = makeRange()->take(0);
  auto ts = std::make_shared<TestSubscriber<int>>();
  flowable->subscribe(ts);

  ts->awaitTerminalEvent();
  EXPECT_EQ(ts->getValueCount(), 0);
  EXPECT_TRUE(ts->isComplete()); // take should terminate the subscriber
}

TEST(MakeFlowableTest, TestTakeOpFinite) {
  auto flowable = makeRange()->take(3);
  auto ts = std::make_shared<TestSubscriber<int>>();
  flowable->subscribe(ts);

  ts->awaitTerminalEvent();
  EXPECT_EQ(ts->getValueCount(), 3);
  EXPECT_EQ(ts->values(), (std::vector<int>{0, 1, 2}));
  EXPECT_TRUE(ts->isComplete());
}

TEST(MakeFlowableTest, TestFiniteRange) {
  auto flowable = makeRange(3);
  auto ts = std::make_shared<TestSubscriber<int>>();
  flowable->subscribe(ts);

  ts->awaitTerminalEvent();
  EXPECT_EQ(ts->getValueCount(), 3);
  EXPECT_EQ(ts->values(), (std::vector<int>{0, 1, 2}));
  EXPECT_TRUE(ts->isComplete());
}

TEST(MakeFlowableTest, ErrorFlowable) {
  auto flowable = Flowable<int>::makeFlowable(
      [](auto sub) { sub->onError(std::runtime_exception{"foo"}); });

  auto ts = std::make_shared<TestSubscriber<int>>();
  flowable->subscribe(ts);
  ts->awaitTerminalEvent();
  EXPECT_TRUE(ts->isError());
  EXPECT_TRUE(ts->errorMessage(), "foo");
}

} // namespace flowable
} // namespace yarpl