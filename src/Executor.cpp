// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/futures/QueuedImmediateExecutor.h>
#include "src/StackTraceUtils.h"
#include "src/SubscriberBase.h"

namespace reactivesocket {

// just instantiating of the template here
template class SubscriberBaseT<Payload>;

folly::Executor& defaultExecutor() {
  static folly::QueuedImmediateExecutor immediateExecutor;
  return immediateExecutor;
}

ExecutorBase::ExecutorBase(folly::Executor& executor, bool startExecutor)
    : executor_(executor) {
  if (!startExecutor) {
    pendingSignals_ = folly::make_unique<PendingSignals>();
  }
}

void ExecutorBase::runInExecutor(folly::Func func) {
  {
    std::lock_guard<std::recursive_mutex> lock(pendingSignalsMutex_);
    VLOG(1) << this << ": " << getStackTrace();
    if (pendingSignals_) {
      pendingSignals_->emplace_back(std::move(func));
      return;
    }
  }
  executor_.add(std::move(func));
}

void ExecutorBase::start() {
  // we are safe to release the lock once the pendingSignals are queued
  std::lock_guard<std::recursive_mutex> lock(pendingSignalsMutex_);

  if (pendingSignals_) {
    auto movedSignals = folly::makeMoveWrapper(std::move(pendingSignals_));
    pendingSignals_ = nullptr; // for the peace of mind
    if (!(*movedSignals)->empty()) {
      // if we are using (default) immediate executor we might be
      // executing the lambda right away
      executor_.add([movedSignals]() mutable {
        for (auto& signal : **movedSignals) {
          // the signal() can call into runInExecutor
          // but since we are using recursive_mutex, it is ok even if it is
          // on the same thread or a different one
          signal();
        }
      });
    }
  }
}

} // reactivesocket
