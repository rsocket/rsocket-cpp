// Copyright 2004-present Facebook. All Rights Reserved.

#include "FlowableBExamples.h"
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/ThreadScheduler.h"

#include "yarpl/FlowableB.h"

using namespace reactivestreams_yarpl;
using namespace yarpl::flowable;
using namespace yarpl;

FlowableB<long> getB() {
  return FlowablesB::range(1, 10);
}

void FlowableBExamples::run() {
  //  getB().subscribe(createSubscriber<long>(
  //      [](auto t) { std::cout << "BValue received: " << t << std::endl; }));

  ThreadScheduler scheduler;

  FlowablesB::range(1, 10)
      .subscribeOn(scheduler)
      //      .map([](auto i) { return "Bhello->" + std::to_string(i); })
      //      .take(3)
      .subscribe(createSubscriber<long>([](auto t) {
        std::cout << "BValue received after scheduling: " << t << std::endl;
      }));

  std::string name;
  std::getline(std::cin, name);

  //  FlowableB<std::string> mapped = FlowablesB::range(1, 10).map(
  //      [](auto i) { return "Bhello->" + std::to_string(i); });
}
