// Copyright 2004-present Facebook. All Rights Reserved.

#include <gtest/gtest.h>
#include <chrono>
#include <iostream>
#include <thread>
#include "yarpl/ThreadScheduler.h"

using namespace yarpl;

TEST(Scheduler, ThreadScheduler_Task) {
  ThreadScheduler scheduler;
  auto worker = scheduler.createWorker();
  worker->schedule([]() {
    std::cout << "doing work on thread id: " << std::this_thread::get_id()
              << std::endl;
  });
  worker->dispose();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
