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

#include <rs/empty.h>
#include <rs/from.h>
#include <rs/just.h>
#include <rs/scan.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Scan") {
  auto running_sum = Scan(3, [](int accum, int v) { return accum + v; });
  static_assert(
      IsPublisher<decltype(running_sum(Empty()))>,
      "Scan stream should be a publisher");

  SECTION("empty") {
    CHECK(GetAll<int>(running_sum(Empty())) == std::vector<int>());
  }

  SECTION("one value") {
    CHECK(GetAll<int>(running_sum(Just(1))) == std::vector<int>({ 4 }));
  }

  SECTION("two values") {
    auto values = From(std::vector<int>{ 1, 2, 3 });
    CHECK(GetAll<int>(running_sum(values)) == std::vector<int>({ 4, 6, 9 }));
  }

  SECTION("use twice") {
    auto values = From(std::vector<int>{ 1, 2, 3 });
    auto stream = running_sum(values);
    CHECK(GetAll<int>(stream) == std::vector<int>({ 4, 6, 9 }));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 4, 6, 9 }));
  }
}

}  // namespace shk
