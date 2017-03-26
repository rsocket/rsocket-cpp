// Copyright 2004-present Facebook. All Rights Reserved.

#include "ObservableExamples.h"
#include <iostream>
#include <string>
#include "yarpl/Observable.h"
#include "yarpl/Observable_Subscription.h"
#include "yarpl/Observable_TestObserver.h"

using namespace yarpl::observable;

void ObservableExamples::run() {
  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;

  class MyObserver : public Observer<int> {
   public:
    void onSubscribe(yarpl::observable::Subscription* subscription) override {}

    void onNext(const int& t) override {
      std::cout << "onNext& " << t << std::endl;
    }

    void onNext(int&& t) override {
      std::cout << "onNext&& " << t << std::endl;
    }

    void onComplete() override {
      std::cout << "onComplete" << std::endl;
    }

    void onError(const std::exception_ptr error) override {}
  };

  // the most basic Observable (and that ignores cancellation)
  Observable<int>::create([](auto oe) {
    oe.onNext(1);
    oe.onNext(2);
    oe.onNext(3);
    oe.onComplete();
  })->subscribe(std::make_unique<MyObserver>());

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;

  // Observable that checks for cancellation
  Observable<int>::create([](auto oe) {
    int i = 1;
    while (!oe.isCancelled()) {
      oe.onNext(i++);
    }
  })
      ->take(3)
      ->subscribe(std::make_unique<MyObserver>());

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;

  // Observable that checks for cancellation
  Observable<int>::create([](auto oe) {
    for (int i = 1; i <= 10 && !oe.isCancelled(); ++i) {
      oe.onNext(i);
    }
    oe.onComplete();
  })
      //      ->take(3)
      ->subscribe(std::make_unique<MyObserver>());

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;

  // an Observable checking for cancellation in a loop
  // NOTE: this will ONLY work synchronously and while in scope
  // as it does not heap allocate
  Observables::unsafeCreate<int>([](auto o) {
    auto s = Subscriptions::create();
    o->onSubscribe(s.get());
    for (int i = 1; !s->isCancelled() && i <= 10; ++i) {
      o->onNext(i);
    }
    o->onComplete();
  })
      ->take(5)
      ->subscribe(std::make_unique<MyObserver>());

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;

  // an Observable that gets a callback on cancel
  // NOTE: this will ONLY work synchronously and while in scope
  // as it does not heap allocate
  Observables::unsafeCreate<int>([](auto o) {
    auto s = Subscriptions::create(
        []() { std::cout << "do cleanup on cancel here" << std::endl; });
    o->onSubscribe(s.get());
    for (int i = 1; !s->isCancelled() && i <= 10; ++i) {
      o->onNext(i);
    }
    o->onComplete();
  })
      ->take(2)
      ->subscribe(std::make_unique<MyObserver>());

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;
}
