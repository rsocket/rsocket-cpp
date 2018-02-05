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

#include <rs/contains.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Contains") {
  auto has_five = Contains(5);

  SECTION("type") {
    auto stream = has_five(Just());
    static_assert(
        IsPublisher<decltype(stream)>,
        "Some stream should be a publisher");
  }

  SECTION("empty") {
    auto stream = has_five(Just());
    CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
  }

  SECTION("no match") {
    SECTION("single element") {
      auto stream = has_five(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }

    SECTION("multiple elements") {
      auto stream = has_five(Just(2, 4, 6, 8));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }
  }

  SECTION("match") {
    SECTION("single element") {
      auto stream = has_five(Just(5));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }

    SECTION("multiple elements") {
      auto stream = has_five(Just(1, 3, 5, 7));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }

    SECTION("infinite stream") {
      auto stream = has_five(InfiniteRange(0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }
  }

  SECTION("custom equality function") {
    auto has_non_five = Contains(5, std::not_equal_to<int>());

    SECTION("no match") {
      SECTION("single element") {
        auto stream = has_non_five(Just(5));
        CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
      }

      SECTION("multiple elements") {
        auto stream = has_non_five(Just(5, 5, 5, 5));
        CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
      }
    }

    SECTION("match") {
      SECTION("single element") {
        auto stream = has_non_five(Just(1));
        CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
      }

      SECTION("multiple elements") {
        auto stream = has_non_five(Just(1, 3, 5, 7));
        CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
      }

      SECTION("infinite stream") {
        auto stream = has_non_five(InfiniteRange(0));
        CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
      }
    }
  }
}

}  // namespace shk
