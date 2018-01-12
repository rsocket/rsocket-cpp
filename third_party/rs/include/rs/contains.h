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

#include <functional>

#include <rs/some.h>

namespace shk {

/**
 * Make a stream that emits exactly one value: True if any of the input elements
 * are equal to the given value to search for, otherwise false.
 */
template <typename Value, typename Compare = std::equal_to<Value>>
auto Contains(Value &&value, const Compare &compare = Compare()) {
  return Some([value = std::forward<Value>(value), compare](const Value &v) {
    return compare(value, v);
  });
}

}  // namespace shk
