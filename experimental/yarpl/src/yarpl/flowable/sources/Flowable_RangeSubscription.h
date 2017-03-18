// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#import <iostream>
#import "reactivestreams/ReactiveStreams.h"
#import "yarpl/Flowable_Subscription.h"

namespace yarpl {
namespace flowable {
namespace sources {

class RangeSubscription : public FlowableSubscription<long> {
 public:
  explicit RangeSubscription(
      long start,
      long count,
      std::unique_ptr<reactivestreams_yarpl::Subscriber<long>> subscriber);

  ~RangeSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY RangeSubscription!!!" << std::endl;
  }

  RangeSubscription(RangeSubscription&&) = delete;
  RangeSubscription(const RangeSubscription&) = delete;
  RangeSubscription& operator=(RangeSubscription&&) = delete;
  RangeSubscription& operator=(const RangeSubscription&) = delete;

  std::tuple<int64_t, bool> emit(int64_t requested) override;

 private:
  int64_t current_;
  int64_t max_;
};
}
}
}
