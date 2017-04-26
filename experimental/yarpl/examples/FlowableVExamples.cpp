// Copyright 2004-present Facebook. All Rights Reserved.

#include "FlowableVExamples.h"

#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "yarpl/ThreadScheduler.h"

#include "yarpl/v/Flowables.h"
#include "yarpl/v/Subscribers.h"

using namespace yarpl;

namespace {

template <typename T>
auto printer() {
  return Subscribers::create<T>(
      [](T value) { std::cout << "  next: " << value << std::endl; },
      2 /* low [optional] batch size for demo */);
}

std::string getThreadId() {
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  return oss.str();
}

void fromPublisherExample() {
  auto onSubscribe = [](Reference<Subscriber<int>> subscriber) {
    class Subscription : public ::yarpl::Subscription {
    public:
      virtual void request(int64_t delta) override {
        // TODO
      }

      virtual void cancel() override {
        // TODO
      }
    };

    Reference<::yarpl::Subscription> subscription(new Subscription);
    subscriber->onSubscribe(subscription);
    subscriber->onNext(1234);
    subscriber->onNext(5678);
    subscriber->onNext(1234);
    subscriber->onComplete();
  };

  Flowables::fromPublisher<int>(std::move(onSubscribe))
      ->subscribe(printer<int>());
}

} // namespace

void FlowableVExamples::run() {
  std::cout << "create a flowable" << std::endl;
  Flowables::range(2, 2);

  std::cout << "just: single value" << std::endl;
  Flowables::just<long>(23)->subscribe(printer<long>());

  std::cout << "just: multiple values." << std::endl;
  Flowables::just<long>({1, 4, 7, 11})->subscribe(printer<long>());

  std::cout << "just: string values." << std::endl;
  Flowables::just<std::string>({"the", "quick", "brown", "fox"})
      ->subscribe(printer<std::string>());

  std::cout << "range operator." << std::endl;
  Flowables::range(1, 4)->subscribe(printer<int64_t>());

  std::cout << "map example: squares" << std::endl;
  Flowables::range(1, 4)
      ->map([](int64_t v) { return v * v; })
      ->subscribe(printer<int64_t>());

  std::cout << "map example: convert to string" << std::endl;
  Flowables::range(1, 4)
      ->map([](int64_t v) { return v * v; })
      ->map([](int64_t v) { return v * v; })
      ->map([](int64_t v) { return std::to_string(v); })
      ->map([](std::string v) { return "-> " + v + " <-"; })
      ->subscribe(printer<std::string>());

  std::cout << "take example: 3 out of 10 items" << std::endl;
  Flowables::range(1, 11)->take(3)->subscribe(printer<int64_t>());

  auto flowable = Flowable<int>::create([total = 0](
      Subscriber<int> & subscriber, int64_t requested) mutable {
    subscriber.onNext(12345678);
    subscriber.onError(std::make_exception_ptr(std::runtime_error("error")));
    return std::make_tuple(int64_t{1}, false);
  });

  auto subscriber = Subscribers::create<int>(
      [](int next) { std::cout << "@next: " << next << std::endl; },
      [](std::exception_ptr eptr) {
        try {
          std::rethrow_exception(eptr);
        } catch (const std::exception& exception) {
          std::cerr << "  exception: " << exception.what() << std::endl;
        } catch (...) {
          std::cerr << "  !unknown exception!" << std::endl;
        }
      },
      [] { std::cout << "Completed." << std::endl; });

  flowable->subscribe(subscriber);

  ThreadScheduler scheduler;

  std::cout << "subscribe_on example" << std::endl;
  Flowables::just({"0: ", "1: ", "2: "})
      ->map([](const char* p) { return std::string(p); })
      ->map([](std::string log) { return log + " on " + getThreadId(); })
      ->subscribeOn(scheduler)
      ->subscribe(printer<std::string>());
  std::cout << "  waiting   on " << getThreadId() << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::cout << "fromPublisher - delegate to onSubscribe" << std::endl;
  fromPublisherExample();
}
