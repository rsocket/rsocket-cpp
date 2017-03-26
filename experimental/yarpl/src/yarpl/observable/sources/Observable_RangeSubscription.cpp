// Copyright 2004-present Facebook. All Rights Reserved.

#include "Observable_RangeSubscription.h"

namespace yarpl {
namespace observable {
namespace sources {

RangeSubscription::RangeSubscription(
    long start,
    long count,
    std::unique_ptr<yarpl::observable::Observer<long>> observer)
    : ObservableSubscription(std::move(observer)),
      current_(start),
      max_(start + count - 1){};

void RangeSubscription::start() {
  for (;
       // below the max (start+count)
       current_ <= max_ &&
       // we are not cancelled
       !isCancelled();
       current_++) {
    //    std::cout << "emitting current " << current_ << std::endl;
    onNext(current_);
  }
  onComplete();
}

} // sources
} // flowable
} // yarpl
