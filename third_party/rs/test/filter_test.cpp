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

#include <rs/just.h>
#include <rs/filter.h>
#include <rs/never.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Filter") {
  auto divisible_by_3 = Filter([](int x) { return (x % 3) == 0; });

  SECTION("empty") {
    SECTION("subscriber is kept") {
      auto stream = divisible_by_3(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
      static_assert(
          IsPublisher<decltype(stream)>,
          "Filter stream should be a publisher");
    }

    SECTION("subscriber is discarded") {
      auto stream = divisible_by_3(Never());
      CHECK(
          GetAll<int>(stream, ElementCount::Unbounded(), false) ==
          (std::vector<int>{}));
    }
  }

  SECTION("one int") {
    SECTION("pass-through") {
      CHECK(
          GetAll<int>(divisible_by_3(Just(3))) ==
          (std::vector<int>{ 3 }));
    }

    SECTION("filtered") {
      CHECK(
          GetAll<int>(divisible_by_3(Just(4))) ==
          (std::vector<int>{ }));
    }
  }

  SECTION("two ints") {
    SECTION("both pass-through") {
      CHECK(
          GetAll<int>(divisible_by_3(Just(3, 9))) ==
          (std::vector<int>{ 3, 9 }));
    }

    SECTION("one filtered") {
      CHECK(
          GetAll<int>(divisible_by_3(Just(4, 9))) ==
          (std::vector<int>{ 9 }));
    }

    SECTION("both filtered") {
      CHECK(
          GetAll<int>(divisible_by_3(Just(1, 5))) ==
          (std::vector<int>{}));
    }
  }

  SECTION("different types") {
    auto is_nonzero = Filter([](auto &&x) { return x != 0; });
    int a;

    CHECK(
        GetAll<int *>(is_nonzero(Just(&a, nullptr))) ==
        (std::vector<int *>{ &a }));

    CHECK(
        GetAll<bool>(is_nonzero(Just(true, false))) ==
        (std::vector<bool>{ true }));
  }

  SECTION("request only one") {
    SECTION("first is matching") {
      CHECK(
          GetAll<int>(
              divisible_by_3(Just(3, 9)),
              ElementCount(1),
              false) ==
          (std::vector<int>{ 3 }));
    }

    SECTION("first is not matching") {
      CHECK(
          GetAll<int>(
              divisible_by_3(Just(4, 9)),
              ElementCount(1),
              true) ==
          (std::vector<int>{ 9 }));
    }
  }

  SECTION("request only two") {
    SECTION("both matching") {
      CHECK(
          GetAll<int>(
              divisible_by_3(Just(0, 12)),
              ElementCount(2)) ==
          (std::vector<int>{ 0, 12 }));
    }

    SECTION("only one matching") {
      CHECK(
          GetAll<int>(
              divisible_by_3(Just(1, 12)),
              ElementCount(2)) ==
          (std::vector<int>{ 12 }));
    }
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(divisible_by_3(Just(3)));
  }

  SECTION("cancel") {
    auto null_subscriber = MakeSubscriber(
        [](int next) { CHECK(!"should not happen"); },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [] { CHECK(!"should not happen"); });

    auto sub = divisible_by_3(InfiniteRange(0))
        .Subscribe(std::move(null_subscriber));
    sub.Cancel();
    // Because the subscription is cancelled, it should not request values
    // from the infinite range (which would never terminate).
    sub.Request(ElementCount::Unbounded());
  }

  SECTION("exceptions") {
    auto fail_on = [](int error_val) {
      return Filter([error_val](int x) {
        if (x == error_val) {
          throw std::runtime_error("fail_on");
        } else {
          return (x % 3) == 0;
        }
      });
    };

    SECTION("empty") {
      CHECK(
          GetAll<int>(fail_on(0)(Just())) ==
          (std::vector<int>{}));
    }

    SECTION("error on first") {
      auto error = GetError(fail_on(0)(Just(0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on second") {
      SECTION("first filtered out") {
        auto error = GetError(fail_on(0)(Just(1, 0)));
        CHECK(GetErrorWhat(error) == "fail_on");
      }

      SECTION("first not filtered out") {
        auto error = GetError(fail_on(0)(Just(3, 0)));
        CHECK(GetErrorWhat(error) == "fail_on");
      }
    }

    SECTION("error on first and second") {
      auto error = GetError(fail_on(0)(Just(0, 0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("source emits value that fails and then fails itself") {
      auto zero_then_fail = fail_on(1)(Just(0, 1));

      // Should only fail once. GetError checks that
      auto error = GetError(fail_on(0)(zero_then_fail));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on second only one requested") {
      SECTION("first filtered out") {
        auto error = GetError(
            fail_on(0)(Just(1, 0)),
            ElementCount(1));
        CHECK(GetErrorWhat(error) == "fail_on");
      }

      SECTION("first not filtered out") {
        CHECK(
            GetAll<int>(
                fail_on(0)(Just(3, 0)),
                ElementCount(1),
                false) ==
            (std::vector<int>{ 3 }));
      }
    }

    SECTION("error on first of infinite") {
      // This will terminate only if the Filter operator actually cancels the
      // underlying InfiniteRange stream.
      auto error = GetError(fail_on(0)(InfiniteRange(0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }
  }
}

}  // namespace shk
