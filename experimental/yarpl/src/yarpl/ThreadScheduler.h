// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include "yarpl/Scheduler.h"

namespace yarpl {

class ThreadScheduler : public Scheduler {
 public:
  ThreadScheduler() {
    std::cout << "Create ThreadScheduler" << std::endl;
  }
  ~ThreadScheduler() {
    // TODO remove this once happy with it
    std::cout << "Destroy ThreadScheduler" << std::endl;
  }
  ThreadScheduler(ThreadScheduler&&) = delete;
  ThreadScheduler(const ThreadScheduler&) = delete;
  ThreadScheduler& operator=(ThreadScheduler&&) = delete;
  ThreadScheduler& operator=(const ThreadScheduler&) = delete;

  std::unique_ptr<Worker> createWorker() override;
};
}
