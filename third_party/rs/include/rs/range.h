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

template <typename Begin>
struct RangeContainer {
 public:
  class RangeIterator {
   public:
    RangeIterator(const Begin &val)
        : val_(val) {}

    Begin operator*() {
      return val_;
    }

    RangeIterator &operator++() {
      ++val_;
      return *this;
    }

    bool operator==(const RangeIterator &other) {
      return val_ == other.val_;
    }

   private:
    Begin val_;
  };

  template <typename BeginT>
  RangeContainer(BeginT &&begin, size_t count)
      : begin_(std::forward<BeginT>(begin)),
        count_(count) {}

  RangeIterator begin() const {
    return RangeIterator(begin_);
  }

  RangeIterator end() const {
    return RangeIterator(begin_ + count_);
  }

 private:
  Begin begin_;
  size_t count_;
};

}  // namespace detail

/**
 * Range takes a start value and a `count` and returns a Publisher that emits
 * `count` incrementing values. For example, `Range(5,2)` generates 5, 6.
 */
template <typename Begin>
auto Range(Begin &&begin, size_t count) {
  return From(
      detail::RangeContainer<typename std::decay<Begin>::type>(
          std::forward<Begin>(begin),
          count));
}

}  // namespace shk
