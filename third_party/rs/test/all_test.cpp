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

#include <rs/all.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("All") {
  auto is_empty = All([](auto &&) { return false; });
  auto all_even = All([](int x) { return (x % 2) == 0; });

  SECTION("type") {
    auto stream = is_empty(Just());
    static_assert(
        IsPublisher<decltype(stream)>,
        "All stream should be a publisher");
  }

  SECTION("empty") {
    auto stream = is_empty(Just());
    CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
  }

  SECTION("with mutable predicate") {
    auto mutable_predicate = All([v = 0](int x) mutable {
      return x == v;
    });

    auto stream = mutable_predicate(Just(0));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
  }

  SECTION("match") {
    SECTION("single element") {
      auto stream = is_empty(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }

    SECTION("multiple elements") {
      auto stream = all_even(Just(0, 2, 4, 9));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }

    SECTION("infinite stream") {
      auto stream = all_even(InfiniteRange(0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }
  }

  SECTION("no match") {
    SECTION("single element") {
      auto stream = all_even(Just(2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }

    SECTION("multiple elements") {
      auto stream = all_even(Just(2, 4, 8, 12));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }
  }
}

}  // namespace shk
