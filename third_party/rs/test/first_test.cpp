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

#include <rs/first.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("First") {
  SECTION("without predicate") {
    SECTION("type") {
      auto stream = First()(Just());
      static_assert(
          IsPublisher<decltype(stream)>,
          "First stream should be a publisher");
    }

    SECTION("empty") {
      auto error = GetError(First()(Just()));
      CHECK(
          GetErrorWhat(error) ==
          "Cannot take the first element of empty stream");
    }

    SECTION("one value") {
      auto stream = First()(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("two values") {
      auto stream = First()(Just(1, 2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("multiple values") {
      auto stream = First()(Just(1, 2, 3, 4, 5));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("infinite stream") {
      auto stream = First()(InfiniteRange(0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 0 }));
    }
  }

  SECTION("with predicate") {
    SECTION("type") {
      auto stream = First([] { return true; })(Just());
      static_assert(
          IsPublisher<decltype(stream)>,
          "First stream should be a publisher");
    }

    auto first_divisible_by_13 = First([](int x) { return (x % 13) == 0; });

    SECTION("empty") {
      auto error = GetError(first_divisible_by_13(Just()));
      CHECK(
          GetErrorWhat(error) ==
          "Cannot take the first element of empty stream");
    }

    SECTION("no match") {
      auto error = GetError(first_divisible_by_13(Just(1, 14, 27)));
      CHECK(
          GetErrorWhat(error) ==
          "Cannot take the first element of empty stream");
    }

    SECTION("one value") {
      auto stream = first_divisible_by_13(Just(13));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 13 }));
    }

    SECTION("first out of two values") {
      auto stream = first_divisible_by_13(Just(13, 2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 13 }));
    }

    SECTION("last out of two values") {
      auto stream = first_divisible_by_13(Just(1, 0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 0 }));
    }

    SECTION("infinite stream") {
      auto stream = first_divisible_by_13(InfiniteRange(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 13 }));
    }
  }
}

}  // namespace shk
