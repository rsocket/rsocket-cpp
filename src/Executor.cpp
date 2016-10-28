// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/futures/QueuedImmediateExecutor.h>
#include "src/SubscriberBase.h"
#include "src/mixins/ExecutorMixin.h"

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

void ExecutorBase::start() {
  if (pendingSignals_) {
    auto movedSignals = folly::makeMoveWrapper(std::move(pendingSignals_));
    if (!(*movedSignals)->empty()) {
      runInExecutor([movedSignals]() mutable {
        for (auto& signal : **movedSignals) {
          signal();
        }
      });
    }
  }
}

} // reactivesocket
