// Copyright 2004-present Facebook. All Rights Reserved.

#include <folly/Memory.h>
#include <folly/futures/InlineExecutor.h>
#include <folly/futures/QueuedImmediateExecutor.h>
#include <folly/io/IOBuf.h>
#include "src/StackTraceUtils.h"
#include "src/SubscriberBase.h"

namespace reactivesocket {

// just instantiating of the template here
template class SubscriberBaseT<Payload>;
template class SubscriberBaseT<folly::IOBuf>;

folly::Executor& defaultExecutor() {
  static folly::QueuedImmediateExecutor executor;
  return executor;
}

folly::Executor& inlineExecutor() {
  static folly::InlineExecutor executor;
  return executor;
}

ExecutorBase::ExecutorBase(folly::Executor& executor) : executor_(executor) {}

void ExecutorBase::runInExecutor(folly::Func func) {
  executor_.add(std::move(func));
}
} // reactivesocket
