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

#include <rs/average.h>
#include <rs/empty.h>
#include <rs/from.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Average") {
  auto average = Average();
  static_assert(
      IsPublisher<decltype(average(Empty()))>,
      "Average stream should be a publisher");

  SECTION("empty") {
    auto error = GetError(average(Empty()));
    CHECK(
        GetErrorWhat(error) == "Cannot compute average value of empty stream");
  }

  SECTION("one value") {
    CHECK(GetOne<double>(average(From(std::vector<int>{ 10 }))) == 10);
  }

  SECTION("two values") {
    CHECK(GetOne<double>(average(From(std::vector<int>{ 10, 21 }))) == 15.5);
  }

  SECTION("int values") {
    CHECK(GetOne<int>(
        Average<int>()(From(std::vector<int>{ 10, 21 }))) == 15);
  }
}

}  // namespace shk
