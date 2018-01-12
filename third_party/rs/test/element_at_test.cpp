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

#include <catch.hpp>

#include <vector>

#include <rs/element_at.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("ElementAt") {
  SECTION("type") {
    auto stream = ElementAt(0)(Just());
    static_assert(
        IsPublisher<decltype(stream)>,
        "ElementAt stream should be a publisher");
  }

  SECTION("empty") {
    auto error = GetError(ElementAt(0)(Just()));
    CHECK(
        GetErrorWhat(error) ==
        "Cannot take the first element of empty stream");
  }

  SECTION("out of bounds") {
    auto error = GetError(ElementAt(1)(Just(1)));
    CHECK(
        GetErrorWhat(error) ==
        "Cannot take the first element of empty stream");
  }

  SECTION("one value") {
    auto stream = ElementAt(0)(Just(1));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
  }

  SECTION("two values") {
    auto stream = ElementAt(0)(Just(1, 2));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
  }

  SECTION("second out of two values") {
    auto stream = ElementAt(1)(Just(1, 2));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 2 }));
  }

  SECTION("infinite stream") {
    auto stream = ElementAt(10)(InfiniteRange(0));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 10 }));
  }
}

}  // namespace shk
