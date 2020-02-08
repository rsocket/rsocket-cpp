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

#include <folly/ExceptionWrapper.h>
#include <folly/synchronization/Baton.h>
#include <gtest/gtest.h>
#include <atomic>

#include "yarpl/Single.h"
#include "yarpl/single/SingleTestObserver.h"
#include "yarpl/test_utils/Tuple.h"

using namespace yarpl::single;

TEST(Single, SingleOnNext) {
  auto a = Single<int>::create([](std::shared_ptr<SingleObserver<int>> obs) {
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
  auto a = Single<int>::create([](std::shared_ptr<SingleObserver<int>> obs) {
    obs->onError(
        folly::exception_wrapper(std::runtime_error("something broke!")));
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
  auto a = Single<int>::create([](std::shared_ptr<SingleObserver<int>> obs) {
    obs->onSubscribe(SingleSubscriptions::empty());
    obs->onSuccess(1);
  });

  auto to = SingleTestObserver<const char*>::create();
  a->map([](int) { return "hello"; })->subscribe(to);
  to->awaitTerminalEvent();
  to->assertOnSuccessValue("hello");
}

TEST(Single, MapWithException) {
  auto single = Singles::just<int>(3)->map([](int n) {
    if (n > 2) {
      throw std::runtime_error{"Too big!"};
    }
    return n;
  });

  auto observer = std::make_shared<SingleTestObserver<int>>();
  single->subscribe(observer);

  observer->assertOnErrorMessage("Too big!");
}
