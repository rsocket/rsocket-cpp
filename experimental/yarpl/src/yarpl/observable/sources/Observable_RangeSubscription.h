// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include "yarpl/Observable_Observer.h"
#include "yarpl/Observable_Subscription.h"

namespace yarpl {
namespace observable {
namespace sources {

class RangeSubscription {
 public:
  explicit RangeSubscription(long start, long count);

  ~RangeSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY RangeSubscription!!!" << std::endl;
  }

  RangeSubscription(RangeSubscription&&) = default;
  RangeSubscription(const RangeSubscription&) = delete;
  RangeSubscription& operator=(RangeSubscription&&) = default;
  RangeSubscription& operator=(const RangeSubscription&) = delete;

  void operator()(std::unique_ptr<yarpl::observable::Observer<long>> s);

 private:
  int64_t current_;
  int64_t max_;
};
}
}
}
