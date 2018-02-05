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

#include <rs/if_empty.h>
#include <rs/just.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("IfEmpty") {
  SECTION("type") {
    auto stream = IfEmpty(Just())(Just());
    static_assert(
        IsPublisher<decltype(stream)>,
        "IfEmpty stream should be a publisher");
  }

  SECTION("non-empty stream") {
    auto null_publisher = MakePublisher([](auto &&subscriber) {
      CHECK(!"should not be subscribed to");
      return MakeSubscription();
    });

    SECTION("one value") {
      auto stream = IfEmpty(null_publisher)(Just(2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 2 }));
    }

    SECTION("several values") {
      auto stream = IfEmpty(null_publisher)(Just(2, 4, 6, 8));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 2, 4, 6, 8 }));
    }

    SECTION("noncopyable value") {
      auto if_empty = IfEmpty(Start([] { return std::make_unique<int>(1); }));
      auto stream = if_empty(Start([] { return std::make_unique<int>(2); }));
      auto result = GetAll<std::unique_ptr<int>>(stream);
      REQUIRE(result.size() == 1);
      REQUIRE(result[0]);
      CHECK(*result[0] == 2);
    }

    SECTION("don't leak the subscriber") {
      CheckLeak(IfEmpty(Just(1))(Just(2)));
    }
  }

  SECTION("empty stream") {
    SECTION("one value") {
      auto stream = IfEmpty(Just(1))(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("several values") {
      auto stream = IfEmpty(Just(1, 2, 3))(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
    }

    SECTION("don't leak the subscriber") {
      CheckLeak(IfEmpty(Just(1))(Just()));
    }
  }
}

}  // namespace shk
