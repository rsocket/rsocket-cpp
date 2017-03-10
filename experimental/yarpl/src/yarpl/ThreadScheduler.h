// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include "yarpl/Scheduler.h"

namespace yarpl {

class ThreadScheduler : public Scheduler {
 public:
  ThreadScheduler(){}
  ThreadScheduler(ThreadScheduler&&) = delete;
  ThreadScheduler(const ThreadScheduler&) = delete;
  ThreadScheduler& operator=(ThreadScheduler&&) = delete;
  ThreadScheduler& operator=(const ThreadScheduler&) = delete;

  std::unique_ptr<Worker> createWorker() override;
};
}