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

#include <rs/concat.h>
#include <rs/just.h>
#include <rs/never.h>
#include <rs/take_while.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("TakeWhile") {
  auto take_while_positive = TakeWhile([](auto x) { return x > 0; });

  SECTION("empty") {
    auto stream = take_while_positive(Just());
    CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    static_assert(
        IsPublisher<decltype(stream)>,
        "TakeWhile stream should be a publisher");
  }

  SECTION("never") {
    auto stream = take_while_positive(Never());
    CHECK(
        GetAll<int>(stream, ElementCount::Unbounded(), false) ==
        (std::vector<int>{}));
  }

  SECTION("one positive") {
    CHECK(
        GetAll<int>(take_while_positive(Just(1))) ==
        (std::vector<int>{ 1 }));
  }

  SECTION("two positive") {
    CHECK(
        GetAll<int>(take_while_positive(Just(1, 2))) ==
        (std::vector<int>{ 1, 2 }));
  }

  SECTION("one negative") {
    CHECK(
        GetAll<int>(take_while_positive(Just(-1))) ==
        (std::vector<int>{}));
  }

  SECTION("two negative") {
    CHECK(
        GetAll<int>(take_while_positive(Just(-1, -2))) ==
        (std::vector<int>{}));
  }

  SECTION("negative then positive") {
    CHECK(
        GetAll<int>(take_while_positive(Just(-1, 1))) ==
        (std::vector<int>{}));
  }

  SECTION("positive then negative then positive") {
    CHECK(
        GetAll<int>(take_while_positive(Just(1, -1, 2))) ==
        (std::vector<int>{ 1 }));
  }

  SECTION("negative then infinite range") {
    // This does not terminate unless the inner stream is cancelled
    auto input_stream = Concat(Just(-1), InfiniteRange(1));
    CHECK(
        GetAll<int>(take_while_positive(input_stream)) ==
        (std::vector<int>{}));
  }

  SECTION("request only one") {
    CHECK(
        GetAll<int>(
            take_while_positive(Just(1, 5)),
            ElementCount(1),
            false) ==
        (std::vector<int>{ 1 }));
  }

  SECTION("request only two") {
    CHECK(
        GetAll<int>(
            take_while_positive(Just(1, 6)),
            ElementCount(2)) ==
        (std::vector<int>{ 1, 6 }));
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(take_while_positive(Just(1)));
  }

  SECTION("cancel") {
    auto null_subscriber = MakeSubscriber(
        [](int next) { CHECK(!"should not happen"); },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [] { CHECK(!"should not happen"); });

    auto sub = take_while_positive(InfiniteRange(0))
        .Subscribe(std::move(null_subscriber));
    sub.Cancel();
    // Because the subscription is cancelled, it should not request values
    // from the infinite range (which would never terminate).
    sub.Request(ElementCount::Unbounded());
  }

  SECTION("exceptions") {
    auto fail_on = [](int error_val) {
      return TakeWhile([error_val](int x) {
        if (x == error_val) {
          throw std::runtime_error("fail_on");
        } else {
          return x > 0;
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

    SECTION("error after cancelled") {
      CHECK(
          GetAll<int>(fail_on(0)(Just(-1, 0))) ==
          (std::vector<int>{}));
    }

    SECTION("error on second") {
      auto error = GetError(fail_on(0)(Just(1, 0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on first and second") {
      auto error = GetError(fail_on(0)(Just(0, 0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("source emits value that fails and then fails itself") {
      auto zero_then_fail = fail_on(2)(Just(1, 2));

      // Should only fail once. GetError checks that
      auto error = GetError(fail_on(1)(zero_then_fail));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on second only one requested") {
      CHECK(
          GetAll<int>(
              fail_on(0)(Just(1, 0)),
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
