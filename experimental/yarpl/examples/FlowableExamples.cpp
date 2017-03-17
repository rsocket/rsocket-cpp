// Copyright 2004-present Facebook. All Rights Reserved.

#include "experimental/yarpl/examples/FlowableExamples.h"
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_Subscriber.h"
#include "yarpl/ThreadScheduler.h"

using namespace reactivestreams_yarpl;
using namespace yarpl::flowable;
using namespace yarpl;

void FlowableExamples::run() {
  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;

  Flowable::range(1, 10)
      ->map([](auto i) { return "hello->" + std::to_string(i); })
      ->take(3)
      ->subscribe(createSubscriber<std::string>(
          [](auto t) { std::cout << "Value received: " << t << std::endl; }));

  std::cout << "--------------- END Example 1" << std::endl;

  std::cout << "Main Thread ID " << std::this_thread::get_id() << std::endl;

  ThreadScheduler scheduler;

  Flowable::range(1, 10)
      ->subscribeOn(scheduler) // put on background thread
      ->map([](auto i) { return "Value received: " + std::to_string(i); })
      ->take(6)
      ->subscribe(createSubscriber<std::string>([](auto t) {
        std::cout << t << " on thread: " << std::this_thread::get_id()
                  << std::endl;
      }));

  // wait to see above async example
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  std::cout << "--------------- END Example 2" << std::endl;

  /* ****************************************************** */

  class MySubscriber : public Subscriber<long> {
   public:
    void onSubscribe(Subscription* subscription) override {
      s_ = subscription;
      requested_ = 10;
      s_->request(10);
    }

    void onNext(const long& t) override {
      acceptAndRequestMoreIfNecessary();
      std::cout << "onNext& " << t << std::endl;
    }

    void onNext(long&& t) override {
      acceptAndRequestMoreIfNecessary();
      std::cout << "onNext&& " << t << std::endl;
    }

    void onComplete() override {
      std::cout << "onComplete " << std::endl;
    }

    void onError(const std::exception_ptr error) override {
      std::cout << "onError " << std::endl;
    }

   private:
    void acceptAndRequestMoreIfNecessary() {
      if (--requested_ == 2) {
        std::cout << "Request more..." << std::endl;
        requested_ += 8;
        s_->request(8);
      }
    }

    Subscription* s_;
    int requested_{0};
  };

  Flowable::range(1, 100)->subscribe(std::make_unique<MySubscriber>());

  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;
}
