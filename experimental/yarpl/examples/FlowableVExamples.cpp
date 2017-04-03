// Copyright 2004-present Facebook. All Rights Reserved.

#include "FlowableVExamples.h"

#include <iostream>
#include <string>
#include <thread>

#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/ThreadScheduler.h"

#include "yarpl/FlowableV.h"

using namespace yarpl;

namespace {

template<typename T>
auto printer() {
  return Subscribers::create<T>([](T value) {
    std::cout << "  next: " << value << std::endl;
  }, 2);
}

}  // namespace

void FlowableVExamples::run() {
  std::cout << "just: single value" << std::endl;
  Flowables::just<long>(23)
      ->subscribe(printer<long>());

  std::cout << "just: multiple values." << std::endl;
  Flowables::just<long>({1, 4, 7, 11})
      ->subscribe(printer<long>());

  std::cout << "just: string values." << std::endl;
  Flowables::just<std::string>({"the", "quick", "brown", "fox"})
      ->subscribe(printer<std::string>());
//  auto next = [](long value) {
//    std::cout << "next: " << value << std::endl;
//  };

//  Flowable<long>::create([](Subscriber<long>& subscriber) {
//    subscriber.onNext(1234);
//  })->subscribe(SubscriberWithOnNext<long, decltype(next)>(std::move(next)));

  std::cout << "range operator." << std::endl;
  Flowables::range(1, 4)->subscribe(printer<int64_t>());
}

//std::unique_ptr<FlowableV<long>> getC() {
//  return FlowablesV::range(1, 10);
//}

//void FlowableVExamples::run() {
//  FlowableV<long>::create([](auto subscriber) {
//    auto subscription = new yarpl::flowable::sources::RangeSubscription(
//        1, 10, std::move(subscriber));
//    subscription->start();
//  })->subscribe(Subscribers::create<long>([](auto t) {
//    std::cout << "Value received: " << t << std::endl;
//  }));

//  FlowablesC::range(1, 5)->subscribe(Subscribers::create<long>(
//      [](auto t) { std::cout << "Value received: " << t << std::endl; }));

//  getC()
//      ->map([](auto i) { return "mapped value => " + std::to_string(i); })
//      ->subscribe(Subscribers::create<std::string>(
//          [](auto t) { std::cout << "from getC => " << t << std::endl; }));

//  FlowablesC::range(1, 5)
//      ->map([](auto i) { return "mapped value => " + std::to_string(i); })
//      ->subscribe(Subscribers::create<std::string>(
//          [](auto t) { std::cout << "Value received: " << t << std::endl; }));

//  FlowablesC::range(1, 5)->take(2)->subscribe(Subscribers::create<long>(
//      [](auto t) { std::cout << "Value received: " << t << std::endl; }));

//  ThreadScheduler scheduler;

//  FlowablesC::range(1, 10)
//      ->subscribeOn(scheduler)
//      ->map([](auto i) {
//        std::this_thread::sleep_for(std::chrono::milliseconds(400));
//        return "mapped->" + std::to_string(i);
//      })
//      ->take(2)
//      ->subscribe(Subscribers::create<std::string>([](auto t) {
//        std::cout << "Value received after scheduling: " << t << std::endl;
//      }));

//  // wait to see above async example
//  /* sleep override */
//  std::this_thread::sleep_for(std::chrono::milliseconds(1300));
//}
