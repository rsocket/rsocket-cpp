// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <folly/Baton.h>

/// Simple implementation of a latch synchronization primitive, for testing.
class Latch {
 public:
  explicit Latch(size_t limit) : limit_{limit} {}

  void wait() {
    baton_.wait();
  }

  bool timed_wait(std::chrono::milliseconds timeout) {
    return baton_.timed_wait(timeout);
  }

  void post() {
    auto const old = count_.fetch_add(1);
    if (old == limit_ - 1) {
      baton_.post();
    }
  }

 private:
  folly::Baton<> baton_;
  std::atomic<size_t> count_{0};
  const size_t limit_{0};
};
