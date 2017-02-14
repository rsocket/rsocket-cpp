
#include "IntStreamSubscription.h"

namespace reactivesocket {

// Emit a stream of ints starting at 0 until number of ints
// emitted matches 'numberToEmit' at which point onComplete()
// will be emitted.
//
// On each invocation will restrict emission to number of requested.
//
// This method has no concurrency since SubscriptionBase
// schedules this on an Executor sequentially
void IntStreamSubscription::requestImpl(size_t n) noexcept {
  LOG(INFO) << "requested=" << n << " currentElem=" << currentElem_
            << " numberToEmit=" << numberToEmit_;

  if (numberToEmit_ == 0) {
    subscriber_->onComplete();
    return;
  }
  for (size_t i = 0; i < n; i++) {
    if (cancelled) {
      LOG(INFO) << "emission stopped by cancellation";
      return;
    }
    subscriber_->onNext(Payload(std::to_string(currentElem_)));
    // currentElem is used to track progress across requestImpl invocations
    currentElem_++;
    // break the loop and complete the stream if numberToEmit_ is matched
    if (currentElem_ == numberToEmit_) {
      subscriber_->onComplete();
      return;
    }
  }
}

void IntStreamSubscription::cancelImpl() noexcept {
  LOG(INFO) << "cancellation received";
  // simple cancellation token (nothing to shut down, just stop next loop)
  cancelled = true;
}
}