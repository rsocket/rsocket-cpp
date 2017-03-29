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
    std::unique_ptr<reactivestreams_yarpl::Subscriber<long>> subscriber)
    : FlowableSubscription(std::move(subscriber)),
      current_(start),
      max_(start + count - 1){};

std::tuple<int64_t, bool> RangeSubscription::emit(int64_t requested) {
  int64_t consumed{0};
  for (;
       // below the max (start+count)
       current_ <= max_ &&
       // we have credits for sending
       requested > 0 &&
       // we are not cancelled
       !isCancelled();
       current_++) {
    //    std::cout << "emitting current " << current_ << std::endl;
    onNext(current_);
    // decrement credit since we used one
    requested--;
    consumed++;
  }
  // it will be >max_ since we suffix increment i.e. current_++
  bool isCompleted = current_ > max_;
  return std::make_tuple(consumed, isCompleted);
}
} // sources
} // flowable
} // yarpl
