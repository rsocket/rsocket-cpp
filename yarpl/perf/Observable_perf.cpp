// Copyright 2004-present Facebook. All Rights Reserved.

#include <benchmark/benchmark.h>
#include <iostream>
#include "yarpl/Observable.h"
#include "yarpl/observable/Observers.h"

using namespace yarpl::observable;

static void Observable_OnNextOne_ConstructOnly(benchmark::State& state) {
  while (state.KeepRunning()) {
    auto a = Observable<int>::create([](yarpl::Reference<Observer<int>> obs) {
      obs->onSubscribe(Subscriptions::empty());
      obs->onNext(1);
      obs->onComplete();
    });
  }
}
BENCHMARK(Observable_OnNextOne_ConstructOnly);

static void Observable_OnNextOne_SubscribeOnly(benchmark::State& state) {
  auto a = Observable<int>::create([](yarpl::Reference<Observer<int>> obs) {
    obs->onSubscribe(Subscriptions::empty());
    obs->onNext(1);
    obs->onComplete();
  });
  while (state.KeepRunning()) {
    a->subscribe(Observers::create<int>([](int value) { /* do nothing */ }));
  }
}
BENCHMARK(Observable_OnNextOne_SubscribeOnly);

static void Observable_OnNextN(benchmark::State& state) {
  auto a =
      Observable<int>::create([&state](yarpl::Reference<Observer<int>> obs) {
        obs->onSubscribe(Subscriptions::empty());
        for (int i = 0; i < state.range(0); i++) {
          obs->onNext(i);
        }
        obs->onComplete();
      });
  while (state.KeepRunning()) {
    a->subscribe(Observers::create<int>([](int value) {}));
  }
}

// Register the function as a benchmark
BENCHMARK(Observable_OnNextN)->Arg(100)->Arg(10000)->Arg(1000000);

BENCHMARK_MAIN()
