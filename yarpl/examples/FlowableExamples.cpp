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

#include "FlowableExamples.h"
#include <folly/io/async/ScopedEventBaseThread.h>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include "yarpl/Flowable.h"

using namespace yarpl::flowable;

namespace {

template <typename T>
auto printer() {
  return Subscriber<T>::create(
      [](T value) { std::cout << "  next: " << value << std::endl; },
      2 /* low [optional] batch size for demo */);
}

std::shared_ptr<Flowable<int64_t>> getData() {
  return Flowable<>::range(2, 5);
}

std::string getThreadId() {
  std::ostringstream oss;
  oss << std::this_thread::get_id();
  return oss.str();
}

void fromPublisherExample() {
  auto onSubscribe = [](std::shared_ptr<Subscriber<int>> subscriber) {
    class Subscription : public ::yarpl::flowable::Subscription {
     public:
      virtual void request(int64_t delta) override {
        // TODO
      }

      virtual void cancel() override {
        // TODO
      }
    };

    auto subscription = std::make_shared<Subscription>();
    subscriber->onSubscribe(subscription);
    subscriber->onNext(1234);
    subscriber->onNext(5678);
    subscriber->onNext(1234);
    subscriber->onComplete();
  };

  Flowable<int>::fromPublisher(std::move(onSubscribe))
      ->subscribe(printer<int>());
}

} // namespace

void FlowableExamples::run() {
  std::cout << "create a flowable" << std::endl;
  Flowable<>::range(2, 2);

  std::cout << "get a flowable from a method" << std::endl;
  getData()->subscribe(printer<int64_t>());

  std::cout << "just: single value" << std::endl;
  Flowable<>::just<long>(23)->subscribe(printer<long>());

  std::cout << "just: multiple values." << std::endl;
  Flowable<>::justN<long>({1, 4, 7, 11})->subscribe(printer<long>());

  std::cout << "just: string values." << std::endl;
  Flowable<>::justN<std::string>({"the", "quick", "brown", "fox"})
      ->subscribe(printer<std::string>());

  std::cout << "range operator." << std::endl;
  Flowable<>::range(1, 4)->subscribe(printer<int64_t>());

  std::cout << "map example: squares" << std::endl;
  Flowable<>::range(1, 4)
      ->map([](int64_t v) { return v * v; })
      ->subscribe(printer<int64_t>());

  std::cout << "map example: convert to string" << std::endl;
  Flowable<>::range(1, 4)
      ->map([](int64_t v) { return v * v; })
      ->map([](int64_t v) { return v * v; })
      ->map([](int64_t v) { return std::to_string(v); })
      ->map([](std::string v) { return "-> " + v + " <-"; })
      ->subscribe(printer<std::string>());

  std::cout << "take example: 3 out of 10 items" << std::endl;
  Flowable<>::range(1, 11)->take(3)->subscribe(printer<int64_t>());

  auto flowable = Flowable<int>::create(
      [total = 0](auto& subscriber, int64_t requested) mutable {
        subscriber.onNext(12345678);
        subscriber.onError(std::runtime_error("error"));
      });

  auto subscriber = Subscriber<int>::create(
      [](int next) { std::cout << "@next: " << next << std::endl; },
      [](folly::exception_wrapper ex) {
        std::cerr << "  exception: " << ex << std::endl;
      },
      [] { std::cout << "Completed." << std::endl; });

  flowable->subscribe(subscriber);

  folly::ScopedEventBaseThread worker;

  std::cout << "subscribe_on example" << std::endl;
  Flowable<>::justN({"0: ", "1: ", "2: "})
      ->map([](const char* p) { return std::string(p); })
      ->map([](std::string log) { return log + " on " + getThreadId(); })
      ->subscribeOn(*worker.getEventBase())
      ->subscribe(printer<std::string>());
  std::cout << "  waiting   on " << getThreadId() << std::endl;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::cout << "fromPublisher - delegate to onSubscribe" << std::endl;
  fromPublisherExample();
}
