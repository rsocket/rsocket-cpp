// Copyright 2004-present Facebook. All Rights Reserved.

#include "ConditionalRequestSubscription.h"
#include <string>

namespace reactivesocket {

// Emit a stream of ints starting at 0 until number of ints
// emitted matches 'numberToEmit' at which point onComplete()
// will be emitted.
//
// On each invocation will restrict emission to number of requested.
//
// This method has no concurrency since SubscriptionBase
// schedules this on an Executor sequentially
void ConditionalRequestSubscription::requestImpl(size_t n) noexcept {
  LOG(INFO) << "requested=" << n << " currentElem=" << currentElem_
            << " numberToEmit=" << numberToEmit_;

  if (numberToEmit_ == 0) {
    subscriber_->onComplete();
    return;
  }
  for (size_t i = 0; i < n; i++) {
    if (cancelled_) {
      LOG(INFO) << "emission stopped by cancellation";
      return;
    }
    std::stringstream ss;
    ss << "Hello " << name_ << " " << currentElem_ << "!";
    std::string s = ss.str();
    subscriber_->onNext(Payload(s));
    // currentElem is used to track progress across requestImpl invocations
    currentElem_++;
    // break the loop and complete the stream if numberToEmit_ is matched
    if (currentElem_ == numberToEmit_) {
      subscriber_->onComplete();
      return;
    }
  }
}

void ConditionalRequestSubscription::cancelImpl() noexcept {
  LOG(INFO) << "cancellation received";
  // simple cancellation token (nothing to shut down, just stop next loop)
  cancelled_ = true;
}

ConditionalRequestSubscription::~ConditionalRequestSubscription() {
  LOG(INFO) << "ConditionalRequestSubscription => destroyed";
}
}
