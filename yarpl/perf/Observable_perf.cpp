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

#include <benchmark/benchmark.h>
#include <iostream>
#include "yarpl/Observable.h"
#include "yarpl/observable/Observers.h"

using namespace yarpl::observable;

static void Observable_OnNextOne_ConstructOnly(benchmark::State& state) {
  while (state.KeepRunning()) {
    auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
      obs->onSubscribe(Subscriptions::empty());
      obs->onNext(1);
      obs->onComplete();
    });
  }
}
BENCHMARK(Observable_OnNextOne_ConstructOnly);

static void Observable_OnNextOne_SubscribeOnly(benchmark::State& state) {
  auto a = Observable<int>::create([](std::shared_ptr<Observer<int>> obs) {
    obs->onSubscribe(Subscriptions::empty());
    obs->onNext(1);
    obs->onComplete();
  });
  while (state.KeepRunning()) {
    a->subscribe(Observer<int>::create([](int /* value */) {}));
  }
}
BENCHMARK(Observable_OnNextOne_SubscribeOnly);

static void Observable_OnNextN(benchmark::State& state) {
  auto a =
      Observable<int>::create([&state](std::shared_ptr<Observer<int>> obs) {
        obs->onSubscribe(Subscriptions::empty());
        for (int i = 0; i < state.range(0); i++) {
          obs->onNext(i);
        }
        obs->onComplete();
      });
  while (state.KeepRunning()) {
    a->subscribe(Observer<int>::create([](int /* value */) {}));
  }
}

// Register the function as a benchmark
BENCHMARK(Observable_OnNextN)->Arg(100)->Arg(10000)->Arg(1000000);

BENCHMARK_MAIN()
