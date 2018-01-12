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

#include <rs/filter.h>
#include <rs/pipe.h>
#include <rs/reduce.h>
#include <rs/take.h>

namespace shk {

/**
 * Make a stream that emits exactly one value: True if any of the input elements
 * matches the predicate, false otherwise.
 */
template <typename Predicate>
auto Some(Predicate &&predicate) {
  return BuildPipe(
      Filter(std::forward<Predicate>(predicate)),
      Take(1),
      Reduce(false, [](bool, auto &&) { return true; }));
}

}  // namespace shk
