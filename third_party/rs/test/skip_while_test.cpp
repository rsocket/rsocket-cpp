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

#include <rs/just.h>
#include <rs/skip_while.h>

#include "test_util.h"

namespace shk {

TEST_CASE("SkipWhile") {
  auto skip_while_even = SkipWhile([](int v) { return (v % 2) == 0; });
  static_assert(
      IsPublisher<decltype(skip_while_even(Just(0)))>,
      "SkipWhile stream should be a publisher");

  SECTION("empty") {
    CHECK(GetAll<int>(skip_while_even(Just())) == std::vector<int>());
  }

  SECTION("single value") {
    CHECK(GetAll<int>(skip_while_even(Just(2))) == std::vector<int>());
    CHECK(GetAll<int>(skip_while_even(Just(1))) == std::vector<int>({ 1 }));
  }

  SECTION("two values") {
    CHECK(
        GetAll<int>(skip_while_even(Just(2, 2))) ==
        std::vector<int>());
    CHECK(
        GetAll<int>(skip_while_even(Just(1, 2))) ==
        std::vector<int>({ 1, 2 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(2, 1))) ==
        std::vector<int>({ 1 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(1, 1))) ==
        std::vector<int>({ 1, 1 }));
  }

  SECTION("three values") {
    CHECK(
        GetAll<int>(skip_while_even(Just(2, 2, 2))) ==
        std::vector<int>());
    CHECK(
        GetAll<int>(skip_while_even(Just(2, 2, 1))) ==
        std::vector<int>({ 1 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(2, 1, 2))) ==
        std::vector<int>({ 1, 2 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(2, 1, 1))) ==
        std::vector<int>({ 1, 1 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(1, 2, 2))) ==
        std::vector<int>({ 1, 2, 2 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(1, 2, 1))) ==
        std::vector<int>({ 1, 2, 1 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(1, 1, 2))) ==
        std::vector<int>({ 1, 1, 2 }));
    CHECK(
        GetAll<int>(skip_while_even(Just(1, 1, 1))) ==
        std::vector<int>({ 1, 1, 1 }));
  }

  SECTION("use twice") {
    auto stream = skip_while_even(Just(2, 1));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 1 }));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 1 }));
  }
}

}  // namespace shk
