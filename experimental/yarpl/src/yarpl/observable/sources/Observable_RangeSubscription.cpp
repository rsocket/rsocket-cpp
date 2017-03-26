// Copyright 2004-present Facebook. All Rights Reserved.

#include "Observable_RangeSubscription.h"

namespace yarpl {
namespace observable {
namespace sources {

RangeSubscription::RangeSubscription(long start, long count)
    : current_(start), max_(start + count - 1){};

void RangeSubscription::operator()(
    std::unique_ptr<yarpl::observable::Observer<long>> observer) {
  auto s = Subscriptions::create();
  observer->onSubscribe(s.get());

  for (;
       // below the max (start+count)
       current_ <= max_ &&
       // we are not cancelled
       !s->isCancelled();
       current_++) {
    //    std::cout << "emitting current " << current_ << std::endl;
    observer->onNext(current_);
  }
  observer->onComplete();
}

} // sources
} // flowable
} // yarpl
