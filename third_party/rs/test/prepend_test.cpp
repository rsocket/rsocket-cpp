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

#include <rs/prepend.h>
#include <rs/empty.h>
#include <rs/just.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Prepend") {
  auto prepend_nothing = Prepend();
  auto prepend_one = Prepend(Just(1));
  auto prepend_two = Prepend(Just(1), Just(2));
  auto prepend_three = Prepend(Just(1), Just(2), Just(3));

  SECTION("no arguments and no input") {
    CHECK(GetAll<int>(prepend_nothing(Empty())) == std::vector<int>({}));
  }

  SECTION("no arguments and some input") {
    CHECK(
        GetAll<int>(prepend_nothing(Just(1, 2))) ==
        std::vector<int>({ 1, 2 }));
  }

  SECTION("single argument and no input") {
    CHECK(GetAll<int>(prepend_one(Empty())) == std::vector<int>({ 1 }));
  }

  SECTION("single argument and some input") {
    CHECK(
        GetAll<int>(prepend_one(Just(42))) ==
        std::vector<int>({ 1, 42 }));
  }

  SECTION("two arguments and no input") {
    CHECK(GetAll<int>(prepend_two(Empty())) == std::vector<int>({ 1, 2 }));
  }

  SECTION("two arguments and some input") {
    CHECK(
        GetAll<int>(prepend_two(Just(42))) ==
        std::vector<int>({ 1, 2, 42 }));
  }

  SECTION("three arguments and no input") {
    CHECK(GetAll<int>(prepend_three(Empty())) == std::vector<int>({ 1, 2, 3 }));
  }

  SECTION("three arguments and some input") {
    CHECK(
        GetAll<int>(prepend_three(Just(42))) ==
        std::vector<int>({ 1, 2, 3, 42 }));
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(prepend_one(Just(42)));
  }
}

}  // namespace shk
