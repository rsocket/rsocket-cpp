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
#include <rs/max.h>
#include <rs/throw.h>

#include "test_util.h"

namespace shk {

TEST_CASE("Max") {
  auto max = Max<int>();
  static_assert(
      IsPublisher<decltype(max(Just(0)))>,
      "Max stream should be a publisher");

  SECTION("empty") {
    auto error = GetError(max(Empty()));
    CHECK(
        GetErrorWhat(error) ==
        "ReduceWithoutInitial invoked with empty stream");
  }

  SECTION("default compare function") {
    SECTION("single value") {
      CHECK(GetOne<int>(max(Just(4))) == 4);
    }

    SECTION("multiple values, biggest last") {
      auto values = From(std::vector<int>({ 1, 2 }));
      CHECK(GetOne<int>(max(values)) == 2);
    }

    SECTION("multiple values, biggest first") {
      auto values = From(std::vector<int>({ 2, 1 }));
      CHECK(GetOne<int>(max(values)) == 2);
    }
  }

  SECTION("custom compare function") {
    auto min = Max<int>(std::greater<int>());

    SECTION("multiple values, biggest last") {
      auto values = From(std::vector<int>({ 1, 2 }));
      CHECK(GetOne<int>(min(values)) == 1);
    }

    SECTION("multiple values, biggest first") {
      auto values = From(std::vector<int>({ 2, 1 }));
      CHECK(GetOne<int>(min(values)) == 1);
    }
  }

  SECTION("failing input stream") {
    auto exception = std::make_exception_ptr(std::runtime_error("test_error"));
    auto error = GetError(max(Throw(exception)));
    CHECK(GetErrorWhat(error) == "test_error");
  }
}

}  // namespace shk
