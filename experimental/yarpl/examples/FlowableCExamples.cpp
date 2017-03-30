// Copyright 2004-present Facebook. All Rights Reserved.

#include "FlowableCExamples.h"
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/ThreadScheduler.h"

#include "yarpl/flowable/sources/Flowable_RangeSubscription.h"

#include "yarpl/FlowableC.h"

using namespace reactivestreams_yarpl;
using namespace yarpl::flowable;
using namespace yarpl::flowableC;
using namespace yarpl;

std::unique_ptr<FlowableC<long>> getC() {
  return FlowablesC::range(1, 10);
}

void FlowableCExamples::run() {
  FlowableC<long>::create([](auto subscriber) {
    auto subscription = new yarpl::flowable::sources::RangeSubscription(
        1, 10, std::move(subscriber));
    subscription->start();
  })->subscribe(Subscribers::create<long>([](auto t) {
    std::cout << "Value received: " << t << std::endl;
  }));

  FlowablesC::range(1, 5)->subscribe(Subscribers::create<long>(
      [](auto t) { std::cout << "Value received: " << t << std::endl; }));

  getC()
      ->map([](auto i) { return "mapped value => " + std::to_string(i); })
      ->subscribe(Subscribers::create<std::string>(
          [](auto t) { std::cout << "from getC => " << t << std::endl; }));

  FlowablesC::range(1, 5)
      ->map([](auto i) { return "mapped value => " + std::to_string(i); })
      ->subscribe(Subscribers::create<std::string>(
          [](auto t) { std::cout << "Value received: " << t << std::endl; }));

  FlowablesC::range(1, 5)->take(2)->subscribe(Subscribers::create<long>(
      [](auto t) { std::cout << "Value received: " << t << std::endl; }));

  ThreadScheduler scheduler;

  FlowablesC::range(1, 10)
      ->subscribeOn(scheduler)
      ->map([](auto i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        return "mapped->" + std::to_string(i);
      })
      ->take(2)
      ->subscribe(Subscribers::create<std::string>([](auto t) {
        std::cout << "Value received after scheduling: " << t << std::endl;
      }));

  // wait to see above async example
  /* sleep override */
  std::this_thread::sleep_for(std::chrono::milliseconds(1300));
}
