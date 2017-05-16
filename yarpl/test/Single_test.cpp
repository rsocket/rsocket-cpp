// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
#include <gtest/gtest.h>
#include <atomic>

#include "yarpl/Single.h"

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

    std::vector<int> v;
    a->subscribe(SingleObservers::create<int>(
        [&v](const int& value) { v.push_back(value); }));

    ASSERT_EQ(std::size_t{1}, Refcounted::objects());
    EXPECT_EQ(v.at(0), 1);
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

    a->subscribe(SingleObservers::create<int>(
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