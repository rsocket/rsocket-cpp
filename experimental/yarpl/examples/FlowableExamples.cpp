// Copyright 2004-present Facebook. All Rights Reserved.

#include "FlowableExamples.h"
#include <thread>
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/Flowable.h"

using namespace reactivestreams_yarpl;
using namespace yarpl::flowable;

using namespace reactivestreams_yarpl;

void FlowableExamples::run() {
  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;

  class MySubscriber : public Subscriber<long> {
   public:
    void onSubscribe(Subscription* subscription) {
      s_ = subscription;
      requested = 10;
      s_->request(10);
    }

    void onNext(const long& t) {
      acceptAndRequestMoreIfNecessary();
      std::cout << "onNext& " << t << std::endl;
    }

    void onNext(long&& t) {
      acceptAndRequestMoreIfNecessary();
      std::cout << "onNext&& " << t << std::endl;
    }

    void onComplete() {
      std::cout << "onComplete " << std::endl;
    }

    void onError(const std::exception_ptr error) {
      std::cout << "onError " << std::endl;
    }

   private:
    Subscription* s_;
    int requested{0};

    void acceptAndRequestMoreIfNecessary() {
      if (--requested == 2) {
        std::cout << "Request more..." << std::endl;
        requested += 8;
        s_->request(8);
      }
    }
  };

  // TODO is there someway to specialize Flowable so the <long> is not needed
  // here?
  Flowable<long>::range(1, 100)->subscribe(std::make_unique<MySubscriber>());

  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;
}
