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

#include <stdexcept>

#include <rs/filter.h>

namespace shk {

/**
 * Takes a stream of values and returns a stream that has the same values in it
 * except for the first `count` ones; they are dropped.
 */
inline auto Skip(size_t count) {
  return Filter([count](auto &&value) mutable {
    if (count == 0) {
      return true;
    } else {
      count--;
      return false;
    }
  });
}

}  // namespace shk
