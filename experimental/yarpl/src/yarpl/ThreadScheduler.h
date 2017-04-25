// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include "yarpl/Scheduler.h"

namespace yarpl {

class ThreadScheduler : public Scheduler {
public:
  ThreadScheduler() {}

  std::unique_ptr<Worker> createWorker() override;

private:
  ThreadScheduler(ThreadScheduler&&) = delete;
  ThreadScheduler(const ThreadScheduler&) = delete;
  ThreadScheduler& operator=(ThreadScheduler&&) = delete;
  ThreadScheduler& operator=(const ThreadScheduler&) = delete;
};
}
