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
#include <rs/if_empty.h>
#include <rs/pipe.h>
#include <rs/take.h>
#include <rs/throw.h>

namespace shk {

/**
 * Takes a stream of values and returns the first of them. If there is no value,
 * the operator fails with an std::out_of_range exception.
 */
inline auto First() {
  static const auto range_error = std::make_exception_ptr(std::out_of_range(
      "Cannot take the first element of empty stream"));

  // Use IfEmpty to ensure that there is at least one value.
  return BuildPipe(
      Take(1),
      IfEmpty(Throw(range_error)));
}

/**
 * Takes a stream of values and returns the first of them that matches a given.
 * predicate. If there is no value that matches the predicate, the operator
 * fails with an std::out_of_range exception.
 */
template <typename Predicate>
auto First(Predicate &&predicate) {
  // Use Last to ensure that there is at least one value.
  return BuildPipe(
      Filter(std::forward<Predicate>(predicate)),
      First());
}

}  // namespace shk
