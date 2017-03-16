// Copyright 2004-present Facebook. All Rights Reserved.

#include "Flowable_RangeSubscription.h"
#include <iostream>
#include "yarpl/flowable/utils/SubscriptionHelper.h"

namespace yarpl {
namespace flowable {
namespace sources {

using namespace yarpl::flowable::internal;

RangeSubscription::RangeSubscription(
    long start,
    long count,
    std::unique_ptr<Subscriber> subscriber)
    : current_(start),
      max_(start + count - 1),
      subscriber_(std::move(subscriber)){};

void RangeSubscription::start() {
  subscriber_->onSubscribe(this);
}

void RangeSubscription::request(int64_t n) {
  if (n <= 0) {
    return;
  }
  int64_t r = SubscriptionHelper::addCredits(&requested_, n);
  if (r <= 0) {
    return;
  }
  auto consumed = tryEmit();
  if (consumed > 0) {
    SubscriptionHelper::consumeCredits(&requested_, consumed);

    // if we've hit the end, or been cancelled, delete ourselves
    // it will be >max_ since we suffix increment i.e. current_++
    if (current_ > max_) {
      subscriber_->onComplete();
      tryDelete();
      return;
    } else if (SubscriptionHelper::isCancelled(&requested_)) {
      tryDelete();
      return;
    }
  }
}

/** do the actual work of emission */
int64_t RangeSubscription::tryEmit() {
  int64_t consumed{0};
  int64_t r{0};
  if (emitting_.fetch_add(1) == 0) {
    // this thread won the 0->1 ticket so will execute
    do {
      // this thread will emitting until emitting_ == 0 again
      for (r = requested_.load();
           // below the max (start+count)
           current_ <= max_ &&
           // we have credits for sending
           r > 0 &&
           // we are not cancelled
           !SubscriptionHelper::isCancelled(&requested_);
           current_++) {
        //    std::cout << "emitting current " << current_ << std::endl;
        subscriber_->onNext(current_);
        // decrement credit since we used one
        r--;
        consumed++;
      }
      // keep looping until emitting) hits 0 (>1 since fetch_add returns value
      // before decrement)
    } while (emitting_.fetch_add(-1) > 1);
    return consumed;
  } else {
    return -1;
  }
}

void RangeSubscription::cancel() {
  if (SubscriptionHelper::addCancel(&requested_)) {
    // if this is the first time calling cancel, try to delete
    // if this thread wins the lock
    tryDelete();
  }
}

void RangeSubscription::tryDelete() {
  // only one thread can call delete
  if (emitting_.fetch_add(1) == 0) {
    // TODO remove this cout once happy with it
    std::cout << "Delete FlowableRange" << std::endl;
    delete this;
  }
}
}
}
}