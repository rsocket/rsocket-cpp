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
#include <rs/skip.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Skip") {
  static_assert(
      IsPublisher<decltype(Skip(0)(Just(0)))>,
      "Skip stream should be a publisher");

  SECTION("empty") {
    CHECK(GetAll<int>(Skip(0)(Just())) == std::vector<int>());
    CHECK(GetAll<int>(Skip(1)(Just())) == std::vector<int>());
    CHECK(GetAll<int>(Skip(2)(Just())) == std::vector<int>());
  }

  SECTION("single value") {
    CHECK(GetAll<int>(Skip(0)(Just(1))) == std::vector<int>({ 1 }));
    CHECK(GetAll<int>(Skip(1)(Just(1))) == std::vector<int>());
    CHECK(GetAll<int>(Skip(2)(Just(1))) == std::vector<int>());
  }

  SECTION("two values") {
    CHECK(GetAll<int>(Skip(0)(Just(1, 2))) == std::vector<int>({ 1, 2 }));
    CHECK(GetAll<int>(Skip(1)(Just(1, 2))) == std::vector<int>({ 2 }));
    CHECK(GetAll<int>(Skip(2)(Just(1, 2))) == std::vector<int>());
  }

  SECTION("three values") {
    CHECK(GetAll<int>(Skip(0)(Just(1, 2, 3))) == std::vector<int>({ 1, 2, 3 }));
    CHECK(GetAll<int>(Skip(1)(Just(1, 2, 3))) == std::vector<int>({ 2, 3 }));
    CHECK(GetAll<int>(Skip(2)(Just(1, 2, 3))) == std::vector<int>({ 3 }));
  }

  SECTION("four values") {
    CHECK(
        GetAll<int>(Skip(0)(Just(1, 2, 3, 4))) ==
        std::vector<int>({ 1, 2, 3, 4 }));
    CHECK(
        GetAll<int>(Skip(1)(Just(1, 2, 3, 4))) ==
        std::vector<int>({ 2, 3, 4 }));
    CHECK(
        GetAll<int>(Skip(2)(Just(1, 2, 3, 4))) ==
        std::vector<int>({ 3, 4 }));
  }

  SECTION("use twice") {
    auto stream = Skip(1)(Just(1, 2));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 2 }));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 2 }));
  }
}

}  // namespace shk
