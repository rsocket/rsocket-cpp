// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <memory>
#include "type_traits.h"
#include "yarpl/Disposable.h"

namespace yarpl {

class Scheduler {
 public:
  ~Scheduler() = default;
  Scheduler(Scheduler&&) = delete;
  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(Scheduler&&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;
  /**
   *
   * Retrieves or creates a new {@link Scheduler.Worker} that represents serial
   * execution of actions.
   * <p>
   * When work is completed it should be disposed using
   * Scheduler::Worker::dispose().
   * <p>
   * Work on a Scheduler::Worker is guaranteed to be sequential.
   *
   * @return a Worker representing a serial queue of actions to be executed
   */
  virtual std::unique_ptr<Worker> createWorker() = 0;
};

class Worker : public yarpl::Disposable {
 public:
  ~Worker() = default;
  Worker(Worker&&) = delete;
  Worker(const Worker&) = delete;
  Worker& operator=(Worker&&) = delete;
  Worker& operator=(const Worker&) = delete;

  template <
      typename F,
      typename = typename std::enable_if<
          std::is_callable<F(T), typename std::result_of<F(T)>::type>::value>::
          type>
  virtual yarpl::Disposable schedule(F&&) = 0;

  // TODO add schehdule methods with delays and periodical execution
};
}