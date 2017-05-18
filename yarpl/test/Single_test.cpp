// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
#include <gtest/gtest.h>
#include <yarpl/include/yarpl/single/SingleTestObserver.h>
#include <atomic>

#include "yarpl/Single.h"
#include "yarpl/single/SingleTestObserver.h"

#include "Tuple.h"

// TODO can we eliminate need to import both of these?
using namespace yarpl;
using namespace yarpl::single;

TEST(Single, SingleOnNext) {
  {
    ASSERT_EQ(std::size_t{0}, Refcounted::objects());
    auto a = Single<int>::create([](Reference<SingleObserver<int>> obs) {
      obs->onSubscribe(SingleSubscriptions::empty());
      obs->onSuccess(1);
    });

    ASSERT_EQ(std::size_t{1}, Refcounted::objects());

    auto to = SingleTestObserver<int>::create();
    a->subscribe(to);
    to->awaitTerminalEvent();
    to->assertOnSuccessValue(1);
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

TEST(Single, OnError) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  {
    std::string errorMessage("DEFAULT->No Error Message");
    auto a = Single<int>::create([](Reference<SingleObserver<int>> obs) {
      try {
        throw std::runtime_error("something broke!");
      } catch (const std::exception&) {
        obs->onError(std::current_exception());
      }
    });

    auto to = SingleTestObserver<int>::create();
    a->subscribe(to);
    to->awaitTerminalEvent();
    to->assertOnErrorMessage("something broke!");
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

TEST(Single, Just) {
  {
    ASSERT_EQ(std::size_t{0}, Refcounted::objects());
    auto a = Singles::just<int>(1);

    auto to = SingleTestObserver<int>::create();
    a->subscribe(to);
    to->awaitTerminalEvent();
    to->assertOnSuccessValue(1);
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

TEST(Single, Error) {
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
  {
    std::string errorMessage("DEFAULT->No Error Message");
    auto a = Singles::error<int>(std::runtime_error("something broke!"));

    auto to = SingleTestObserver<int>::create();
    a->subscribe(to);
    to->awaitTerminalEvent();
    to->assertOnErrorMessage("something broke!");
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}

TEST(Single, SingleMap) {
  {
    ASSERT_EQ(std::size_t{0}, Refcounted::objects());
    auto a = Single<int>::create([](Reference<SingleObserver<int>> obs) {
      obs->onSubscribe(SingleSubscriptions::empty());
      obs->onSuccess(1);
    });

    ASSERT_EQ(std::size_t{1}, Refcounted::objects());

    auto to = SingleTestObserver<const char*>::create();
    a->map([](int v) { return "hello"; })->subscribe(to);
    to->awaitTerminalEvent();
    to->assertOnSuccessValue("hello");
  }
  ASSERT_EQ(std::size_t{0}, Refcounted::objects());
}
