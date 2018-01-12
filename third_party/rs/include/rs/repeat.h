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
struct RepeatContainer {
 public:
  class RepeatIterator {
   public:
    RepeatIterator(const Value &value, size_t count)
        : value_(value),
          count_(count) {}

    Value operator*() {
      return value_;
    }

    RepeatIterator &operator++() {
      count_--;
      return *this;
    }

    bool operator==(const RepeatIterator &other) {
      return count_ == other.count_;
    }

   private:
    const Value &value_;
    size_t count_;
  };

  template <typename ValueT>
  RepeatContainer(ValueT &&value, size_t count)
      : value_(std::forward<ValueT>(value)),
        count_(count) {}

  RepeatIterator begin() const {
    return RepeatIterator(value_, count_);
  }

  RepeatIterator end() const {
    return RepeatIterator(value_, 0);
  }

 private:
  Value value_;
  size_t count_;
};

}  // namespace detail

/**
 * Repeat takes a value value and a `count` and returns a Publisher that emits
 * `count` equal values. For example, `Repeat(5, 3)` generates [5, 5, 5].
 */
template <typename Begin>
auto Repeat(Begin &&begin, size_t count) {
  return From(
      detail::RepeatContainer<typename std::decay<Begin>::type>(
          std::forward<Begin>(begin),
          count));
}

}  // namespace shk
