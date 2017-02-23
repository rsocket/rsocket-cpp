#include <iostream>
#include <string>

#include <yarpl/Observable.h>

using namespace yarpl::observable;

class ObservableExamples {
public:
  static void run();
};

void ObservableExamples::run() {
  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;
  // subscription
  std::unique_ptr<Subscription> s =
      Subscription::create([]() { std::cout << "cancel" << std::endl; });
  std::cout << "Subscription cancelled: " << s->isCanceled() << std::endl;
  s->cancel();
  std::cout << "Subscription cancelled: " << s->isCanceled() << std::endl;

  // observer with next callback
  auto o = Observer<int>::create(
      [](auto v) { std::cout << "received value " << v << std::endl; });
  o->onNext(5);

  // observer with all 3 callbacks
  auto o2 = Observer<int>::create(
      [](auto v) { std::cout << "received value " << v << std::endl; },
      [](auto e) { std::cout << "received error " << e.what() << std::endl; },
      []() { std::cout << "completed" << std::endl; });
  o2->onNext(42);
  o2->onComplete();

  // inline class with custom Observer implementation
  class MyObserver : Observer<std::string> {
    std::unique_ptr<Subscription> subscription;
    int count;

  public:
    void onNext(const std::string &value) {
      count++;
      std::cout << "MyObserver::onNext " << value
                << "  subscription: " << subscription->isCanceled()
                << std::endl;
      if (count >= 2) {
        subscription->cancel();
      }
    }

    void onError(const std::exception &exception) {}

    void onComplete() { std::cout << "MyObserver::onComplete" << std::endl; }

    void onSubscribe(std::unique_ptr<Subscription> ptr) {
      std::cout << "MyObserver::onSubscribe" << std::endl;
      subscription = std::move(ptr);
    }
  } myObserver;

  myObserver.onSubscribe(Subscription::create());
  myObserver.onNext("hello");
  myObserver.onNext("world");
  myObserver.onNext("!");
  myObserver.onComplete();

  // create an Observable

  auto a = Observable<int>::create([](std::unique_ptr<Observer<int>> obs) {
    obs->onSubscribe(Subscription::create([]() { std::cout << "cancelled!"; }));
    obs->onNext(1);
    obs->onNext(2);
    obs->onComplete();
  });

  a->subscribe(Observer<int>::create(
      [](auto value) { std::cout << "Observed => " << value << std::endl; }));

  std::cout << "---------------ObservableExamples::run-----------------"
            << std::endl;
}
