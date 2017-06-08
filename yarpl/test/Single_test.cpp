// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Baton.h>
#include <gtest/gtest.h>
#include <atomic>

#include "yarpl/Single.h"
#include "yarpl/single/SingleTestObserver.h"

#include "Tuple.h"

// TODO can we eliminate need to import both of these?
using namespace yarpl;
using namespace yarpl::single;

TEST(Single, SingleOnNext) {
  auto a = Single<int>::create([](Reference<SingleObserver<int>> obs) {
    obs->onSubscribe(SingleSubscriptions::empty());
    obs->onSuccess(1);
  });

  auto to = SingleTestObserver<int>::create();
  a->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue(1);
}

TEST(Single, OnError) {
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

TEST(Single, Just) {
  auto a = Singles::just<int>(1);

  auto to = SingleTestObserver<int>::create();
  a->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue(1);
}

TEST(Single, Error) {
  std::string errorMessage("DEFAULT->No Error Message");
  auto a = Singles::error<int>(std::runtime_error("something broke!"));

  auto to = SingleTestObserver<int>::create();
  a->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnErrorMessage("something broke!");
}

TEST(Single, SingleMap) {
  auto a = Single<int>::create([](Reference<SingleObserver<int>> obs) {
    obs->onSubscribe(SingleSubscriptions::empty());
    obs->onSuccess(1);
  });

  auto to = SingleTestObserver<const char*>::create();
  a->map([](int v) { return "hello"; })->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue("hello");
}
