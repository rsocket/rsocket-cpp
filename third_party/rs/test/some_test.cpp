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

#include <rs/some.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Some") {
  auto any = Some([](auto &&) { return true; });
  auto even = Some([](int x) { return (x % 2) == 0; });

  SECTION("type") {
    auto stream = any(Just());
    static_assert(
        IsPublisher<decltype(stream)>,
        "Some stream should be a publisher");
  }

  SECTION("empty") {
    auto stream = any(Just());
    CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
  }

  SECTION("no match") {
    SECTION("single element") {
      auto stream = even(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }

    SECTION("multiple elements") {
      auto stream = even(Just(1, 3, 5, 9));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ false }));
    }
  }

  SECTION("match") {
    SECTION("single element") {
      auto stream = even(Just(2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }

    SECTION("multiple elements") {
      auto stream = even(Just(1, 3, 8, 9));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }

    SECTION("infinite stream") {
      auto stream = even(InfiniteRange(0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ true }));
    }
  }
}

}  // namespace shk
