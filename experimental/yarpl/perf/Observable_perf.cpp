#include <benchmark/benchmark.h>
#include <yarpl/Observable.h>
#include <iostream>

using namespace yarpl::observable;

static void Observable_OnNextOne_ConstructOnly(benchmark::State &state) {
  while (state.KeepRunning()) {
    auto a = Observable<int>::create([](std::unique_ptr<Observer<int>> obs) {
      obs->onSubscribe(Subscription::create());
      obs->onNext(1);
      obs->onComplete();
    });
  }
}
BENCHMARK(Observable_OnNextOne_ConstructOnly);

static void Observable_OnNextOne_SubscribeOnly(benchmark::State &state) {
  auto a = Observable<int>::create([](std::unique_ptr<Observer<int>> obs) {
    obs->onSubscribe(Subscription::create());
    obs->onNext(1);
    obs->onComplete();
  });
  while (state.KeepRunning()) {
    a->subscribe(Observer<int>::create([](int value) { /* do nothing */ }));
  }
}
BENCHMARK(Observable_OnNextOne_SubscribeOnly);

static void Observable_OnNextN(benchmark::State &state) {
  auto a = Observable<int>::create([&state](std::unique_ptr<Observer<int>> obs) {
    obs->onSubscribe(Subscription::create());
    for (int i = 0; i < state.range_x(); i++) {
      obs->onNext(i);
    }
    obs->onComplete();
  });
  while (state.KeepRunning()) {
    a->subscribe(Observer<int>::create([](int value) { /* do nothing */ }));
  }
}

// Register the function as a benchmark
BENCHMARK(Observable_OnNextN)->Arg(100)->Arg(10000)->Arg(1000000);
