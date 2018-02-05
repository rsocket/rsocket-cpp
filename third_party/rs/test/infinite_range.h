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

#include <rs/from.h>

namespace shk {
namespace detail {

template <typename Value>
struct InfiniteRangeContainer {
 public:
  class Iterator {
   public:
    Iterator() = default;

    explicit Iterator(const Value &value)
        : value_(value) {}

    Value operator*() {
      return value_;
    }

    Iterator &operator++() {
      ++value_;
      return *this;
    }

    bool operator==(const Iterator &other) {
      return false;
    }

   private:
    Value value_;
  };

  template <typename ValueT>
  InfiniteRangeContainer(ValueT &&value)
      : value_(std::forward<Value>(value)) {}

  Iterator begin() const {
    return Iterator(value_);
  }

  Iterator end() const {
    return Iterator(value_);
  }

 private:
  Value value_;
};

}  // namespace detail

/**
 * InfiniteRange takes a start value and returns a Publisher that emits an
 * infinite stream of values, incremented by one each time.
 *
 * This is useful for testing operators that should cancel their sources.
 */
template <typename Value>
auto InfiniteRange(Value &&value) {
  return From(
      detail::InfiniteRangeContainer<typename std::decay<Value>::type>(
          std::forward<Value>(value)));
}

}  // namespace shk
