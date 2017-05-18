// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include <thread>
#include "yarpl/schedulers/ThreadScheduler.h"

using namespace yarpl;

TEST(Scheduler, ThreadScheduler_Task) {
  ThreadScheduler scheduler;
  auto worker = scheduler.createWorker();
  worker->schedule([]() {
    std::cout << "doing work on thread id: " << std::this_thread::get_id()
              << std::endl;
  });
  worker->dispose();

  // TODO add condition variable into task above instead of sleep
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  // TODO add validation of above, right now just testing it doesn't blow up
}
