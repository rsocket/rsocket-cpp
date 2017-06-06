// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <cassert>
#include <cstddef>
#include <limits>

namespace rsocket {

class AllowanceSemaphore {
 public:
  using ValueType = size_t;

  AllowanceSemaphore() = default;

  explicit AllowanceSemaphore(ValueType initialValue) : value_(initialValue) {}

  bool tryAcquire(ValueType n = 1) {
    if (!canAcquire(n)) {
      return false;
    }
    value_ -= n;
    return true;
  }

  ValueType release(ValueType n) {
    auto old_value = value_;
    value_ += n;
    if (old_value > value_) {
      value_ = max();
    }
    return old_value;
  }

  bool canAcquire(ValueType n = 1) const {
    return value_ >= n;
  }

  ValueType drain() {
    return drainWithLimit(max());
  }

  ValueType drainWithLimit(ValueType limit) {
    if (limit > value_) {
      limit = value_;
    }
    value_ -= limit;
    return limit;
  }

  explicit operator bool() const {
    return value_;
  }

  static ValueType max() {
    return std::numeric_limits<ValueType>::max();
  }

 private:
  static_assert(
      !std::numeric_limits<ValueType>::is_signed,
      "Allowance representation must be an unsigned type");
  static_assert(
      std::numeric_limits<ValueType>::is_integer,
      "Allowance representation must be an integer type");
  ValueType value_{0};
};
} // reactivesocket
