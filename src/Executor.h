// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Executor.h>
#include <memory>
#include <mutex>
#include <vector>

namespace reactivesocket {

folly::Executor& defaultExecutor();

class ExecutorBase {
 public:
  // if startExecutor == false then all incoming signals will by queued
  // until start() method is called
  explicit ExecutorBase(
      folly::Executor& executor = defaultExecutor(),
      bool startExecutor = true);

  /// We start in a queueing mode, where it merely queues signal
  /// deliveries until ::start is invoked.
  ///
  /// Calling into this method may deliver all enqueued signals immediately.
  void start();

 protected:
  void runInExecutor(folly::Func func);

 private:
  using PendingSignals = std::vector<folly::Func>;
  std::recursive_mutex pendingSignalsMutex_;
  std::unique_ptr<PendingSignals> pendingSignals_;
  folly::Executor& executor_;
};

} // reactivesocket
