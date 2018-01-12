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

#include <rs/range.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Range") {
  SECTION("construct") {
    auto stream = Range(0, 0);
    static_assert(
        IsPublisher<decltype(stream)>,
        "Range stream should be a publisher");
  }

  SECTION("empty range") {
    auto stream = Range(1, 0);
    CHECK(GetAll<int>(stream) == std::vector<int>({}));
  }

  SECTION("range with one value") {
    auto stream = Range(13, 1);
    CHECK(GetAll<int>(stream) == std::vector<int>({ 13 }));
  }

  SECTION("range with two values") {
    auto stream = Range(15, 2);
    CHECK(GetAll<int>(stream) == std::vector<int>({ 15, 16 }));
  }

  SECTION("don't take reference to its parameter") {
    // Can happen if you forget to std::decay

    int val = 13;
    auto stream = Range(val, 1);
    val++;
    CHECK(GetAll<int>(stream) == std::vector<int>({ 13 }));
  }
}

}  // namespace shk
