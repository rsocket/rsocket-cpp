// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <functional>
#include <memory>
#include "yarpl/Disposable.h"
#include "yarpl/utils/type_traits.h"

namespace yarpl {

class Worker : public yarpl::Disposable {
 public:
  Worker() {}
  Worker(Worker&&) = delete;
  Worker(const Worker&) = delete;
  Worker& operator=(Worker&&) = delete;
  Worker& operator=(const Worker&) = delete;

  //  template <
  //      typename F,
  //      typename = typename std::enable_if<
  //          std::is_callable<F(), typename
  //          std::result_of<F()>::type>::value>::
  //          type>
  //  virtual yarpl::Disposable schedule(F&&) = 0; // TODO can't do this, so how
  //  do we allow different impls?

  virtual std::unique_ptr<yarpl::Disposable> schedule(
      std::function<void()>&&) = 0;

  virtual void dispose() override = 0;

  virtual bool isDisposed() override = 0;

  // TODO add schedule methods with delays and periodical execution
};

class Scheduler {
 public:
  Scheduler() {}
  virtual ~Scheduler() = default;
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
}
