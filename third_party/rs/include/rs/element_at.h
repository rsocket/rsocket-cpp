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

#include <rs/first.h>
#include <rs/pipe.h>
#include <rs/take.h>

namespace shk {

/**
 * Takes a stream of values and returns a stream that has only the element at
 * the specified index. If the stream finishes after emitting fewer than
 * index + 1 elements, this fails with an std::out_of_range exception.
 */
inline auto ElementAt(size_t index) {
  return BuildPipe(
      Take(index + 1),  // This is needed to handle infinite streams
      First([index](auto &&) mutable {
        return index-- == 0;
      }));
}

}  // namespace shk
