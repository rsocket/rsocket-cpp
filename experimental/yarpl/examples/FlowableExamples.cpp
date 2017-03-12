// Copyright 2004-present Facebook. All Rights Reserved.

#include "FlowableExamples.h"
#include "reactivestreams/ReactiveStreams.h"
#include "yarpl/flowable.h"

using namespace reactivestreams_yarpl;
using namespace yarpl::flowable;

void FlowableExamples::run() {
  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;

  class MySubscriber : public Subscriber<int> {
    Subscription<int>* theSubscription_;

   public:
    MySubscriber() {
      std::cout << "MySubscriber CREATED" << std::endl;
    }
    ~MySubscriber() {
      std::cout << "MySubscriber DESTROYED" << std::endl;
    }
    void onNext(const int& value) {
      std::cout << " MySubscriber onNext& received " << value << std::endl;
    }
    void onNext(int&& value) {
      std::cout << " MySubscriber onNext&& received " << value << std::endl;
    }
    void onError(const std::exception_ptr e) {}
    void onComplete() {}
    void onSubscribe(Subscription<int>* s) {
      theSubscription_ = s;
      theSubscription_->request(10);
    }
  };

  class MySubscription : public Subscription<int> {
   public:
    MySubscription(std::unique_ptr<Subscriber<int>> s) : s_(std::move(s)) {
      std::cout << "MySubscription CREATED" << std::endl;
    };
    ~MySubscription() {
      std::cout << "MySubscription DESTROYED" << std::endl;
    }
    void cancel() {}
    void request(long n) {
      s_->onNext(1);
    }
    void start() {
      s_->onSubscribe(this);
    }

   private:
    std::unique_ptr<Subscriber<int>> s_;
  };

  {
    std::cout << "--------- MySubscriber? " << std::endl;
    auto b = std::make_unique<MySubscriber>();
    b->onComplete();
  }
  std::cout << "--------- MySubscriber? " << std::endl;

  {
    auto f = Flowable<int>::create([](std::unique_ptr<Subscriber<int>> s) {
      std::cout << "Flowable onSubscribe START" << std::endl;
      auto subscription = std::make_unique<MySubscription>(std::move(s));
      subscription->start();
      std::cout << "Flowable onSubscribe END" << std::endl;
    });

    f->subscribe(std::make_unique<MySubscriber>());
  }
  std::cout << "--------- All destroyed? " << std::endl;

  std::cout << "---------------FlowableExamples::run-----------------"
            << std::endl;
}
