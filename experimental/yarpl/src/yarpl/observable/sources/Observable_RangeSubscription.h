// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <iostream>
#include "yarpl/Observable_Observer.h"
#include "yarpl/Observable_Subscription.h"

namespace yarpl {
namespace observable {
namespace sources {

class RangeSubscription : public ObservableSubscription<long> {
 public:
  explicit RangeSubscription(
      long start,
      long count,
      std::unique_ptr<yarpl::observable::Observer<long>> observer);

  ~RangeSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY RangeSubscription!!!" << std::endl;
  }

  RangeSubscription(RangeSubscription&&) = default;
  RangeSubscription(const RangeSubscription&) = delete;
  RangeSubscription& operator=(RangeSubscription&&) = default;
  RangeSubscription& operator=(const RangeSubscription&) = delete;

  void start() override;

 private:
  int64_t current_;
  int64_t max_;
};
}
}
}
