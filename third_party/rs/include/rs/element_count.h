// Copyright 2017 Per Grön. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <limits>
#include <stdexcept>

namespace shk {
namespace detail {

inline long long ElementCountAdd(long long a, long long b) {
  if (b > 0 && a > std::numeric_limits<long long>::max() - b) {
    // Overflow
    return std::numeric_limits<long long>::max();
  } else if (b < 0) {
    if (a == std::numeric_limits<long long>::max()) {
      // Subtracting from unbounded is still unbounded
      return std::numeric_limits<long long>::max();
    } else if (a < std::numeric_limits<long long>::min() - b) {
      // Negative overflow
      throw std::range_error("Negative integer overflow");
    }
  }
  return a + b;
}

inline long long ElementCountSubtract(long long a, long long b) {
  if (b < 0 && a > std::numeric_limits<long long>::max() + b) {
    // Overflow
    return std::numeric_limits<long long>::max();
  } else if (b > 0) {
    if (a == std::numeric_limits<long long>::max()) {
      // Subtracting from unbounded is still unbounded
      return std::numeric_limits<long long>::max();
    } else if (a < std::numeric_limits<long long>::min() + b) {
      // Negative overflow
      throw std::range_error("Negative integer overflow");
    }
  }
  return a - b;
}

}  // namespace detail

/**
 * ElementCount behaves more or less like a long long, except that the maximum
 * value is considered "unbounded", and adding or removing from unbounded is
 * still unbounded.
 *
 * This is useful when implementing the Subscription.Request method: without
 * this class it is very easy to get integer overflow bugs.
 */
class ElementCount {
 public:
  using Value = long long;

  ElementCount() : count_(0) {}

  explicit ElementCount(Value count)
      : count_(count) {}

  bool IsUnbounded() const {
    return count_ == std::numeric_limits<Value>::max();
  }

  Value Get() const {
    return count_;
  }

  static ElementCount Unbounded() {
    return ElementCount(std::numeric_limits<Value>::max());
  }

  ElementCount &operator=(Value value) {
    count_ = value;
    return *this;
  }

  ElementCount &operator++() {
    // Prefix increment
    if (!IsUnbounded()) {
      count_++;
    }
    return *this;
  }

  ElementCount &operator--() {
    // Prefix decrement
    if (count_ == std::numeric_limits<Value>::min()) {
      throw std::range_error("Cannot decrement the smallest possible value");
    }
    if (!IsUnbounded()) {
      count_--;
    }
    return *this;
  }

  ElementCount operator++(int) {
    // Postfix increment
    auto copy = *this;
    ++(*this);
    return copy;
  }

  ElementCount operator--(int) {
    // Postfix decrement
    auto copy = *this;
    --(*this);
    return copy;
  }

  ElementCount &operator+=(ElementCount other) {
    count_ = detail::ElementCountAdd(count_, other.Get());
    return *this;
  }

  ElementCount &operator+=(Value other) {
    count_ = detail::ElementCountAdd(count_, other);
    return *this;
  }

  ElementCount &operator-=(ElementCount other) {
    count_ = detail::ElementCountSubtract(count_, other.Get());
    return *this;
  }

  ElementCount &operator-=(Value other) {
    count_ = detail::ElementCountSubtract(count_, other);
    return *this;
  }

 private:
  Value count_;
};

inline ElementCount operator+(ElementCount a, ElementCount b) {
  return ElementCount(detail::ElementCountAdd(a.Get(), b.Get()));
}

inline ElementCount operator+(ElementCount a, ElementCount::Value b) {
  return ElementCount(detail::ElementCountAdd(a.Get(), b));
}

inline ElementCount operator+(ElementCount::Value a, ElementCount b) {
  return ElementCount(detail::ElementCountAdd(a, b.Get()));
}

inline ElementCount operator-(ElementCount a, ElementCount b) {
  return ElementCount(detail::ElementCountSubtract(a.Get(), b.Get()));
}

inline ElementCount operator-(ElementCount a, ElementCount::Value b) {
  return ElementCount(detail::ElementCountSubtract(a.Get(), b));
}

inline ElementCount operator-(ElementCount::Value a, ElementCount b) {
  return ElementCount(detail::ElementCountSubtract(a, b.Get()));
}

inline bool operator==(ElementCount a, ElementCount b) {
  return a.Get() == b.Get();
}

inline bool operator==(ElementCount a, ElementCount::Value b) {
  return a.Get() == b;
}

inline bool operator==(ElementCount::Value a, ElementCount b) {
  return a == b.Get();
}

inline bool operator!=(ElementCount a, ElementCount b) {
  return !(a == b);
}

inline bool operator!=(ElementCount a, ElementCount::Value b) {
  return !(a == b);
}

inline bool operator!=(ElementCount::Value a, ElementCount b) {
  return !(a == b);
}

inline bool operator<(ElementCount a, ElementCount b) {
  return a.Get() < b.Get();
}

inline bool operator<(ElementCount a, ElementCount::Value b) {
  return a.Get() < b;
}

inline bool operator<(ElementCount::Value a, ElementCount b) {
  return a < b.Get();
}

inline bool operator>(ElementCount a, ElementCount b) {
  return a.Get() > b.Get();
}

inline bool operator>(ElementCount a, ElementCount::Value b) {
  return a.Get() > b;
}

inline bool operator>(ElementCount::Value a, ElementCount b) {
  return a > b.Get();
}

inline bool operator<=(ElementCount a, ElementCount b) {
  return a.Get() <= b.Get();
}

inline bool operator<=(ElementCount a, ElementCount::Value b) {
  return a.Get() <= b;
}

inline bool operator<=(ElementCount::Value a, ElementCount b) {
  return a <= b.Get();
}

inline bool operator>=(ElementCount a, ElementCount b) {
  return a.Get() >= b.Get();
}

inline bool operator>=(ElementCount a, ElementCount::Value b) {
  return a.Get() >= b;
}

inline bool operator>=(ElementCount::Value a, ElementCount b) {
  return a >= b.Get();
}

}  // namespace shk
