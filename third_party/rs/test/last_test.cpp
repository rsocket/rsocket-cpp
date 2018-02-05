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
#include <rs/last.h>
#include <rs/throw.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Last") {
  SECTION("construct") {
    auto stream = Last<int>();
    static_assert(
        IsPublisher<decltype(stream(Just(0)))>,
        "Last stream should be a publisher");
  }

  SECTION("empty") {
    auto error = GetError(Last<int>()(Empty()));
    CHECK(
        GetErrorWhat(error) ==
        "ReduceWithoutInitial invoked with empty stream");
  }

  SECTION("single value") {
    CHECK(GetOne<int>(Last<int>()(Just(4))) == 4);
  }

  SECTION("multiple values") {
    auto values = From(std::vector<int>({ 1, 2 }));
    CHECK(GetOne<int>(Last<int>()(values)) == 2);
  }

  SECTION("failing input stream") {
    auto exception = std::make_exception_ptr(std::runtime_error("test_error"));
    auto error = GetError(Last<int>()(Throw(exception)));
    CHECK(GetErrorWhat(error) == "test_error");
  }
}

}  // namespace shk
