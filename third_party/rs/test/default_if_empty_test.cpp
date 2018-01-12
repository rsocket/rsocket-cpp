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

#include <string>
#include <vector>

#include <rs/default_if_empty.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("DefaultIfEmpty") {
  SECTION("type") {
    auto stream = DefaultIfEmpty(Just())(Just());
    static_assert(
        IsPublisher<decltype(stream)>,
        "DefaultIfEmpty stream should be a publisher");
  }

  SECTION("non-empty stream") {
    SECTION("one value") {
      auto stream = DefaultIfEmpty(1)(Just(2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 2 }));
    }

    SECTION("several values") {
      auto stream = DefaultIfEmpty(1)(Just(2, 4, 6, 8));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 2, 4, 6, 8 }));
    }
  }

  SECTION("empty stream") {
    SECTION("one value") {
      auto stream = DefaultIfEmpty(1)(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("several values") {
      auto stream = DefaultIfEmpty(1, 2, 3)(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
    }
  }
}

}  // namespace shk
