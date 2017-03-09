#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <reactivestreams/subscriber.h>
#include <yarpl/flowable.h>
#include <yarpl/scheduler.h>

namespace {

using reactivestreams::Subscriber;

auto make_subscriber(int state) {
  return Subscriber<int>::create(
      [state](int i) mutable {
        std::cout << "  state: " << ++state << ", i = " << i << std::endl;
      },
      [](const std::exception_ptr eptr) {
        try {
          std::rethrow_exception(eptr);
        } catch (const std::exception& exception) {
          std::cerr << "  exception: " << exception.what() << std::endl;
        } catch (...) {
          std::cerr << "  !unknown exception!" << std::endl;
        }
      },
      []() { std::cout << "  complete." << std::endl; });
}

class Scheduler : public yarpl::Scheduler {
 public:
  virtual ~Scheduler() = default;
  virtual void schedule(std::function<void()>&& action) {
    std::thread(action).detach();
  }
};

}  // namespace

int main(int, char* []) {
  using yarpl::Flowable;

  std::cout << "@ do nothing." << std::endl;
  Flowable<int>::create([=](Subscriber<int>&) {
  })->subscribe(make_subscriber(1000));

  std::cout << "@ throw an error." << std::endl;
  Flowable<int>::create([=](Subscriber<int>&) {
    throw std::runtime_error("Don't do that.");
  })->subscribe(make_subscriber(1000));

  std::cout << "@ multiple next calls." << std::endl;
  Flowable<int>::create([=](Subscriber<int>& subscriber) {
    for (int i = 0; i < 3; ++i) subscriber.on_next(100 + i);
  })->subscribe(make_subscriber(500));

  std::cout << "@ table of squares of squares." << std::endl;
  Flowable<int>::create([=](Subscriber<int>& subscriber) {
    for (int i = 0; i < 5; ++i) subscriber.on_next(i);
  })->map([](int x) { return x * x; })
    ->map([](int x) { return x * x; })
    ->subscribe(make_subscriber(600));

  Scheduler scheduler;

  auto background =
      Flowable<int>::create([=](Subscriber<int>& subscriber) {
        for (int i = 0; i < 5; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(200));
          subscriber.on_next(i);
        }
      })->map([](int x) { return x * x * x; });

  background->subscribe_on(make_subscriber(500), scheduler);
  std::cout << "@ asynchronous, background flowable started." << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(2));
  return 0;
}
