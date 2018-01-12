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

#include <rs/concat.h>
#include <rs/empty.h>
#include <rs/just.h>
#include <rs/end_with.h>

#include "test_util.h"

namespace shk {

TEST_CASE("EndWith") {
  SECTION("EndWith") {
    auto end_with_nothing = EndWith();
    auto end_with_one = EndWith(1);
    auto end_with_two = EndWith(1, 2);

    SECTION("no prefix and no input") {
      CHECK(GetAll<int>(end_with_nothing(Empty())) == std::vector<int>({}));
    }

    SECTION("no prefix and some input") {
      CHECK(
          GetAll<int>(end_with_nothing(Just(42))) ==
          std::vector<int>({ 42 }));
    }

    SECTION("single value prefix and no input") {
      CHECK(GetAll<int>(end_with_one(Empty())) == std::vector<int>({ 1 }));
    }

    SECTION("single value prefix and some input") {
      CHECK(
          GetAll<int>(end_with_one(Just(42))) ==
          std::vector<int>({ 42, 1 }));
    }

    SECTION("two value prefix and no input") {
      CHECK(GetAll<int>(end_with_two(Empty())) == std::vector<int>({ 1, 2 }));
    }

    SECTION("two value prefix and some input") {
      CHECK(
          GetAll<int>(end_with_two(Just(42))) ==
          std::vector<int>({ 42, 1, 2 }));
    }

    SECTION("don't leak the subscriber") {
      CheckLeak(end_with_one(Just(42)));
    }
  }

  SECTION("EndWithGet") {
    auto end_with_nothing = EndWithGet();
    auto end_with_one = EndWithGet([] { return 1; });
    auto end_with_two = EndWithGet([] { return 1; }, [] { return 2; });

    SECTION("noncopyable prefix and no input") {
      auto end_with_unique = EndWithGet([] {
        return std::make_unique<int>(1);
      });
      auto result = GetAll<std::unique_ptr<int>>(end_with_unique(Empty()));
      REQUIRE(result.size() == 1);
      REQUIRE(result[0]);
      CHECK(*result[0] == 1);
    }

    SECTION("no prefix and no input") {
      CHECK(GetAll<int>(end_with_nothing(Empty())) == std::vector<int>({}));
    }

    SECTION("no prefix and some input") {
      CHECK(
          GetAll<int>(end_with_nothing(Just(42))) ==
          std::vector<int>({ 42 }));
    }

    SECTION("single value prefix and no input") {
      CHECK(GetAll<int>(end_with_one(Empty())) == std::vector<int>({ 1 }));
    }

    SECTION("single value prefix and some input") {
      CHECK(
          GetAll<int>(end_with_one(Just(42))) ==
          std::vector<int>({ 42, 1 }));
    }

    SECTION("two value prefix and no input") {
      CHECK(GetAll<int>(end_with_two(Empty())) == std::vector<int>({ 1, 2 }));
    }

    SECTION("two value prefix and some input") {
      CHECK(
          GetAll<int>(end_with_two(Just(42))) ==
          std::vector<int>({ 42, 1, 2 }));
    }

    SECTION("don't leak the subscriber") {
      CheckLeak(end_with_one(Just(42)));
    }
  }
}

}  // namespace shk
