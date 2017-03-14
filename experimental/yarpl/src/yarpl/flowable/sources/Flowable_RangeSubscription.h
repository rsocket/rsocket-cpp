// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#import <iostream>
#import "reactivestreams/ReactiveStreams.h"

namespace yarpl {
namespace flowable {
namespace sources {

using Subscriber = reactivestreams_yarpl::Subscriber<long>;
using Subscription = reactivestreams_yarpl::Subscription;

class RangeSubscription : public Subscription {
 public:
  explicit RangeSubscription(
      long start,
      long count,
      std::unique_ptr<Subscriber> subscriber);

  ~RangeSubscription() {
    // TODO remove this once happy with it
    std::cout << "DESTROY RangeSubscription!!!" << std::endl;
  }

  RangeSubscription(RangeSubscription&&) = delete;
  RangeSubscription(const RangeSubscription&) = delete;
  RangeSubscription& operator=(RangeSubscription&&) = delete;
  RangeSubscription& operator=(const RangeSubscription&) = delete;

  void start();
  void request(int64_t n) override;
  void cancel() override;

 private:
  int64_t current_;
  int64_t max_;
  std::atomic_ushort emitting_{0};
  std::unique_ptr<Subscriber> subscriber_;
  std::atomic<std::int64_t> requested_{0};

  /**
   * thread-safe method to try and emit in response to request(n) being called.
   *
   * Only one thread will win.
   *
   * @return int64_t of consumed credits
   */
  int64_t tryEmit();
  void tryDelete();
};
}
}
}
