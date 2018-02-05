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

#include <rs/filter.h>
#include <rs/just.h>
#include <rs/pipe.h>
#include <rs/start_with.h>
#include <rs/take.h>
#include <rs/throw.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Take") {
  auto null_subscriber = MakeSubscriber(
      [](int next) { CHECK(!"should not happen"); },
      [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
      [] { CHECK(!"should not happen"); });

  SECTION("take from empty") {
    SECTION("take 0") {
      auto stream = Take(0)(Just());
      CHECK(
          GetAll<int>(stream) ==
          (std::vector<int>{}));
      static_assert(
          IsPublisher<decltype(stream)>,
          "Take stream should be a publisher");
    }

    SECTION("take 1") {
      auto stream = Take(1)(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take -1") {
      auto stream = Take(-1)(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take infinite") {
      auto stream = Take(ElementCount::Unbounded())(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }
  }

  SECTION("take from single element") {
    SECTION("take 0") {
      auto stream = Take(0)(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take 1") {
      auto stream = Take(1)(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("take -1") {
      auto stream = Take(-1)(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take infinite") {
      auto stream = Take(ElementCount::Unbounded())(Just(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }
  }

  SECTION("take from multiple elements") {
    SECTION("take 0") {
      auto stream = Take(0)(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take 1") {
      auto stream = Take(1)(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("take 2") {
      auto stream = Take(2)(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2 }));
    }

    SECTION("take 3") {
      auto stream = Take(3)(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
    }

    SECTION("take 4") {
      auto stream = Take(4)(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
    }

    SECTION("take -1") {
      auto stream = Take(-1)(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take infinite") {
      auto stream = Take(ElementCount::Unbounded())(Just(1, 2, 3));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
    }
  }

  SECTION("take from infinite stream") {
    SECTION("take 1") {
      auto stream = Pipe(
          InfiniteRange(1),
          Take(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }

    SECTION("take 2") {
      auto stream = Pipe(
          InfiniteRange(1),
          Take(2));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2 }));
    }

    SECTION("take 0 from infinite range") {
      auto stream = Pipe(
          InfiniteRange(1),
          Take(0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take 0 from filtered infinite range") {
      auto stream = Pipe(
          InfiniteRange(1),
          Filter([](int x) { return false; }),
          Take(0));
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("take 1 from filtered infinite range") {
      auto stream = Pipe(
          InfiniteRange(1),
          Filter([](int x) { return x == 1; }),
          Take(1));
      CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
    }
  }

  SECTION("use stream multiple times") {
    auto stream = Take(2)(Just(1, 2, 3));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2 }));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2 }));
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(Take(1)(Just(1, 2)));
  }

  SECTION("cancel") {
    auto sub = Pipe(InfiniteRange(0), Take(1))
        .Subscribe(std::move(null_subscriber));
    sub.Cancel();
    // Because the subscription is cancelled, it should not request values
    // from the infinite range (which would never terminate).
    sub.Request(ElementCount::Unbounded());
  }

  SECTION("exceptions") {
    SECTION("failing input") {
      auto stream = Pipe(
          Throw(std::runtime_error("test")),
          Take(1));

      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test");
    }

    SECTION("input that fails later") {
      auto stream = Pipe(
          Throw(std::runtime_error("test")),
          StartWith(0),
          Take(1));

      CHECK(GetAll<int>(stream) == (std::vector<int>{ 0 }));
    }
  }
}

}  // namespace shk
