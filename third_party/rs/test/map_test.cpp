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

#include <rs/from.h>
#include <rs/map.h>
#include <rs/never.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Map") {
  auto add_self = Map([](auto x) { return x + x; });

  SECTION("empty") {
    SECTION("subscriber is kept") {
      auto stream = add_self(From(std::vector<int>{}));
      CHECK(
          GetAll<int>(stream) ==
          (std::vector<int>{}));
      static_assert(
          IsPublisher<decltype(stream)>,
          "Mapped stream should be a publisher");
    }

    SECTION("subscriber is discarded") {
      auto stream = add_self(Never());
      CHECK(
          GetAll<int>(stream, ElementCount::Unbounded(), false) ==
          (std::vector<int>{}));
    }
  }

  SECTION("one int") {
    CHECK(
        GetAll<int>(add_self(From(std::vector<int>{ 1 }))) ==
        (std::vector<int>{ 2 }));
  }

  SECTION("two ints") {
    CHECK(
        GetAll<int>(add_self(From(std::vector<int>{ 1, 5 }))) ==
        (std::vector<int>{ 2, 10 }));
  }

  SECTION("one string") {
    // To check that add_self can be used with different types (it is also used
    // with ints).
    CHECK(
        GetAll<std::string>(
            add_self(From(std::vector<std::string>{ "a" }))) ==
        (std::vector<std::string>{ "aa" }));
  }

  SECTION("request only one") {
    CHECK(
        GetAll<int>(
            add_self(From(std::vector<int>{ 1, 5 })),
            ElementCount(1),
            false) ==
        (std::vector<int>{ 2 }));
  }

  SECTION("request only two") {
    CHECK(
        GetAll<int>(
            add_self(From(std::vector<int>{ 1, 6 })),
            ElementCount(2)) ==
        (std::vector<int>{ 2, 12 }));
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(add_self(From(std::vector<int>{})));
  }

  SECTION("cancel") {
    auto null_subscriber = MakeSubscriber(
        [](int next) { CHECK(!"should not happen"); },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [] { CHECK(!"should not happen"); });

    auto sub = add_self(InfiniteRange(0)).Subscribe(std::move(null_subscriber));
    sub.Cancel();
    // Because the subscription is cancelled, it should not request values
    // from the infinite range (which would never terminate).
    sub.Request(ElementCount::Unbounded());
  }

  SECTION("exceptions") {
    auto fail_on = [](int error_val) {
      return Map([error_val](int x) {
        if (x == error_val) {
          throw std::runtime_error("fail_on");
        } else {
          return x;
        }
      });
    };

    SECTION("empty") {
      CHECK(
          GetAll<int>(fail_on(0)(From(std::vector<int>{}))) ==
          (std::vector<int>{}));
    }

    SECTION("error on first") {
      auto error = GetError(fail_on(0)(From(std::vector<int>{ 0 })));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on second") {
      auto error = GetError(fail_on(0)(From(std::vector<int>{ 1, 0 })));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on first and second") {
      auto error = GetError(fail_on(0)(From(std::vector<int>{ 0, 0 })));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("source emits value that fails and then fails itself") {
      auto zero_then_fail = fail_on(1)(From(std::vector<int>{ 0, 1 }));

      // Should only fail once. GetError checks that
      auto error = GetError(fail_on(0)(zero_then_fail));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on second only one requested") {
      CHECK(
          GetAll<int>(
              fail_on(0)(From(std::vector<int>{ 1, 0 })),
              ElementCount(1),
              false) ==
          (std::vector<int>{ 1 }));
    }

    SECTION("error on first of infinite") {
      // This will terminate only if the Map operator actually cancels the
      // underlying InfiniteRange stream.
      auto error = GetError(fail_on(0)(InfiniteRange(0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }
  }
}

}  // namespace shk
