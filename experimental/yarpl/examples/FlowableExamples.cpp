// Copyright 2004-present Facebook. All Rights Reserved.

#include "experimental/yarpl/examples/FlowableExamples.h"
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"
#include "yarpl/Flowable_Subscriber.h"

using namespace reactivestreams_yarpl;
using namespace yarpl::flowable;

void FlowableExamples::run() {
  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;

  Flowable::range(1, 100)
      ->map([](auto i) { return "hello->" + std::to_string(i); })
      ->take(10)
      ->subscribe(createSubscriber<std::string>(
          [](auto t) { std::cout << "Value received: " << t << std::endl; }));

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
