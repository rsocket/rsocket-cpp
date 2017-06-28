// Copyright 2004-present Facebook. All Rights Reserved.

#include "ObservableExamples.h"
#include <iostream>
#include <string>
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/Observable.h"
#include "yarpl/Subscriptions.h"
#include "yarpl/TestObserver.h"

using namespace yarpl::observable;
using yarpl::flowable::Flowable;
using yarpl::flowable::Flowables;
using yarpl::flowable::Subscribers;

void ObservableExamples::run() {
  //  std::cout << "---------------ObservableExamples::run-----------------"
  //            << std::endl;
  //
  //  class MyObserver : public Observer<int> {
  //   public:
  //    void onSubscribe(yarpl::observable::Subscription* subscription) override
  //    {}
  //
  //    void onNext(const int& t) override {
  //      std::cout << "onNext& " << t << std::endl;
  //    }
  //
  //    void onNext(int&& t) override {
  //      std::cout << "onNext&& " << t << std::endl;
  //    }
  //
  //    void onComplete() override {
  //      std::cout << "onComplete" << std::endl;
  //    }
  //
  //    void onError(std::exception_ptr) override {}
  //  };
  //
  //  // the most basic Observable (and that ignores cancellation)
  //  Observable<int>::create([](auto oe) {
  //    oe.onNext(1);
  //    oe.onNext(2);
  //    oe.onNext(3);
  //    oe.onComplete();
  //  })->subscribe(std::make_unique<MyObserver>());
  //
  //  std::cout << "---------------ObservableExamples::run-----------------"
  //            << std::endl;
  //
  //  // Observable that checks for cancellation
  //  Observable<int>::create([](auto oe) {
  //    int i = 1;
  //    while (!oe.isCancelled()) {
  //      oe.onNext(i++);
  //    }
  //  })
  //      ->take(3)
  //      ->subscribe(std::make_unique<MyObserver>());
  //
  //  std::cout << "---------------ObservableExamples::run-----------------"
  //            << std::endl;
  //
  //  // Observable that checks for cancellation
  //  Observable<int>::create([](auto oe) {
  //    for (int i = 1; i <= 10 && !oe.isCancelled(); ++i) {
  //      oe.onNext(i);
  //    }
  //    oe.onComplete();
  //  })
  //      //      ->take(3)
  //      ->subscribe(std::make_unique<MyObserver>());
  //
  //  std::cout << "---------------ObservableExamples::run-----------------"
  //            << std::endl;
  //
  //  // an Observable checking for cancellation in a loop
  //  // NOTE: this will ONLY work synchronously and while in scope
  //  // as it does not heap allocate
  //  Observables::unsafeCreate<int>([](auto o) {
  //    auto s = Subscriptions::create();
  //    o->onSubscribe(s.get());
  //    for (int i = 1; !s->isCancelled() && i <= 10; ++i) {
  //      o->onNext(i);
  //    }
  //    o->onComplete();
  //  })
  //      ->take(5)
  //      ->subscribe(std::make_unique<MyObserver>());
  //
  //  std::cout << "---------------ObservableExamples::run-----------------"
  //            << std::endl;
  //
  //  // an Observable that gets a callback on cancel
  //  // NOTE: this will ONLY work synchronously and while in scope
  //  // as it does not heap allocate
  //  Observables::unsafeCreate<int>([](auto o) {
  //    auto s = Subscriptions::create(
  //        []() { std::cout << "do cleanup on cancel here" << std::endl; });
  //    o->onSubscribe(s.get());
  //    for (int i = 1; !s->isCancelled() && i <= 10; ++i) {
  //      o->onNext(i);
  //    }
  //    o->onComplete();
  //  })
  //      ->take(2)
  //      ->subscribe(std::make_unique<MyObserver>());
  //
  //  std::cout << "---------------ObservableExamples::run-----------------"
  //            << std::endl;

  auto o = Observable<int>::create([](auto oe) {
    oe.onNext(1);
    oe.onNext(2);
    oe.onNext(3);
    oe.onComplete();
  });

  auto f = o->toFlowable(BackpressureStrategy::DROP);
  f->subscribe(Subscribers::create<int>(
      [](auto t) { std::cout << "Observable->Flowable: " << t << std::endl; }));

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;

  // test again with same Observable, but with flow control this time

  class MySubscriber : public reactivestreams_yarpl::Subscriber<int> {
   public:
    void onSubscribe(
        reactivestreams_yarpl::Subscription* subscription) override {
      s_ = subscription;
      // only request 2, when it will emit 3
      s_->request(2);
    }

    void onNext(const int& t) override {
      std::cout << "onNext& " << t << std::endl;
    }

    void onComplete() override {
      std::cout << "onComplete " << std::endl;
    }

    void onError(std::exception_ptr) override {
      std::cout << "onError " << std::endl;
    }

   private:
    reactivestreams_yarpl::Subscription* s_;
  };

  o->toFlowable(BackpressureStrategy::DROP)
      ->subscribe(std::make_unique<MySubscriber>());

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;
}
