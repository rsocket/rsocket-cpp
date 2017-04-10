// Copyright 2004-present Facebook. All Rights Reserved.

#include "yarpl/ThreadScheduler.h"

#include <atomic>
#include <functional>
#include <iostream>
#include <thread>

#include "yarpl/Disposable.h"

/**
 * A VERY BAD implementation of Scheduler.
 * This spawns a thread for *every* schedule event.
 * This also means it breaks the contract of ensuring sequential
 * execution on a single Worker.
 *
 * And it does nothing with disposal.
 */
// TODO fix this mess by finishing a proper implementation
namespace yarpl {

class ADisposable : public yarpl::Disposable {
  void dispose() override {}

  bool isDisposed() override {
    return false;
  }
};

class ThreadWorker : public Worker {
 public:
  ThreadWorker() {
    std::cout << "Create ThreadWorker" << std::endl;
  }
  ~ThreadWorker() {
    std::cout << "DESTROYING ThreadWorker!" << std::endl;
  }

  std::unique_ptr<yarpl::Disposable> schedule(
      std::function<void()>&& task) override {
    std::thread([task = std::move(task)]() { task(); }).detach();
    return std::make_unique<ADisposable>();
  }

  void dispose() override {
    isDisposed_.store(true);
  }

  bool isDisposed() override {
    return isDisposed_;
  }

 private:
  std::atomic_bool isDisposed_{false};
  //  std::thread loop_;
};

std::unique_ptr<Worker> ThreadScheduler::createWorker() {
  return std::make_unique<ThreadWorker>();
}
}
