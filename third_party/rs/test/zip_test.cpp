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

#include <stdexcept>
#include <vector>

#include <rs/concat.h>
#include <rs/empty.h>
#include <rs/just.h>
#include <rs/never.h>
#include <rs/throw.h>
#include <rs/zip.h>

#include "backpressure_violator.h"
#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Zip") {
  SECTION("type") {
    static_assert(
        IsPublisher<decltype(Zip<std::tuple<>>())>,
        "Zip stream should be a publisher");
  }

  SECTION("subscription is default constructible") {
    auto stream = Zip<std::tuple<int>>(Just(1));
    decltype(stream.Subscribe(MakeNonDefaultConstructibleSubscriber())) sub;
    sub.Request(ElementCount(1));
    sub.Cancel();
  }

  SECTION("no streams") {
    SECTION("output") {
      auto stream = Zip<std::tuple<>>();
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("should instantly complete") {
      auto stream = Zip<std::tuple<>>();

      bool is_done = false;
      stream.Subscribe(MakeSubscriber(
          [](auto &&val) {
            CHECK(!"OnNext should not be called");
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [&is_done] {
            CHECK(!is_done);
            is_done = true;
          }));
      CHECK(is_done);
    }
  }

  SECTION("one empty stream") {
    SECTION("output") {
      auto stream = Zip<std::tuple<int>>(Just());
      CHECK(
          GetAll<std::tuple<int>>(stream) ==
          (std::vector<std::tuple<int>>{}));
    }

    SECTION("should instantly complete") {
      auto stream = Zip<std::tuple<int>>(Just());

      bool is_done = false;
      stream.Subscribe(MakeSubscriber(
          [](auto &&val) {
            CHECK(!"OnNext should not be called");
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [&is_done] {
            CHECK(!is_done);
            is_done = true;
          }));
      CHECK(is_done);
    }
  }

  SECTION("two empty streams") {
    SECTION("Empty") {
      auto stream = Zip<std::tuple<int, std::string>>(Empty(), Empty());
      CHECK(
          (GetAll<std::tuple<int, std::string>>(stream)) ==
          (std::vector<std::tuple<int, std::string>>{}));
    }

    SECTION("Just") {
      auto stream = Zip<std::tuple<int, std::string>>(Just(), Just());
      CHECK(
          (GetAll<std::tuple<int, std::string>>(stream)) ==
          (std::vector<std::tuple<int, std::string>>{}));
    }
  }

  SECTION("one stream with one value") {
    auto stream = Zip<std::tuple<int>>(Just(1));
    CHECK(
        GetAll<std::tuple<int>>(stream) ==
        (std::vector<std::tuple<int>>{ 1 }));
  }

  SECTION("one stream with two values") {
    auto stream = Zip<std::tuple<int>>(Just(1, 2));
    CHECK(
        GetAll<std::tuple<int>>(stream) ==
        (std::vector<std::tuple<int>>{ 1, 2 }));
  }

  SECTION("two streams with one value") {
    auto stream = Zip<std::tuple<int, int>>(Just(1), Just(2));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream)) ==
        (std::vector<std::tuple<int, int>>{ std::make_tuple(1, 2) }));
  }

  SECTION("two streams with two values") {
    auto stream = Zip<std::tuple<int, int>>(Just(1, 2), Just(3, 4));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream)) ==
        (std::vector<std::tuple<int, int>>{
            std::make_tuple(1, 3),
            std::make_tuple(2, 4) }));
  }

  SECTION("two streams where first is longer") {
    auto stream = Zip<std::tuple<int, int>>(Just(1, 2), Just(3));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream)) ==
        (std::vector<std::tuple<int, int>>{
            std::make_tuple(1, 3) }));
  }

  SECTION("two streams where second is longer") {
    auto stream = Zip<std::tuple<int, int>>(Just(1), Just(2, 3));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream)) ==
        (std::vector<std::tuple<int, int>>{
            std::make_tuple(1, 2) }));
  }

  SECTION("one empty stream, the other infinite") {
    auto stream = Zip<std::tuple<int, int>>(Just(), InfiniteRange(0));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream)) ==
        (std::vector<std::tuple<int, int>>{}));
  }

  SECTION("one never stream, the other infinite") {
    // This test attempts to trigger an infinite loop / crash that can happen if
    // Zip has an unbounded buffer.
    auto stream = Zip<std::tuple<int, int>>(Never(), InfiniteRange(0));
    auto sub = stream.Subscribe(MakeSubscriber());
    sub.Request(ElementCount::Unbounded());
  }

  SECTION("two streams with two values, request one") {
    auto stream = Zip<std::tuple<int, int>>(Just(1, 2), Just(3, 4));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream, ElementCount(1), false)) ==
        (std::vector<std::tuple<int, int>>{
            std::make_tuple(1, 3) }));
  }

  SECTION("two streams with two values, request two") {
    auto stream = Zip<std::tuple<int, int>>(Just(1, 2), Just(3, 4));
    CHECK(
        (GetAll<std::tuple<int, int>>(stream, ElementCount(2))) ==
        (std::vector<std::tuple<int, int>>{
            std::make_tuple(1, 3),
            std::make_tuple(2, 4) }));
  }

  SECTION("requesting parts of stream at a time") {
    SECTION("input streams that synchronously emit values") {
      for (int i = 1; i <= 2; i++) {
        auto stream = Zip<std::tuple<int, int>>(
            Just(1, 2, 3, 4, 5),
            Just(6, 7, 8, 9, 10));

        std::vector<std::tuple<int, int>> result;
        bool is_done = false;
        auto sub = stream.Subscribe(MakeSubscriber(
            [&is_done, &result](auto &&val) {
              CHECK(!is_done);
              result.emplace_back(std::forward<decltype(val)>(val));
            },
            [](std::exception_ptr &&error) {
              CHECK(!"OnError should not be called");
            },
            [&is_done] {
              CHECK(!is_done);
              is_done = true;
            }));

        for (int j = 0; !is_done && j < 200; j++) {
          sub.Request(ElementCount(i));
        }
        CHECK(is_done);
        CHECK(
            result ==
            (std::vector<std::tuple<int, int>>{
                std::make_tuple(1, 6),
                std::make_tuple(2, 7),
                std::make_tuple(3, 8),
                std::make_tuple(4, 9),
                std::make_tuple(5, 10) }));
      }
    }

    SECTION("input streams that asynchronously emit values") {
      // This test tries to make sure that the Request method never Requests
      // more elements than it has buffer for.
      auto sub = Zip<std::tuple<int, int>>(Just(1, 2), Never())
          .Subscribe(MakeSubscriber(
              [](std::tuple<int, int> value) {
                CHECK(!"stream should not emit any value");
              },
              [](std::exception_ptr &&error) {
                CHECK(!"stream should not fail");
              },
              [] {
                CHECK(!"stream should not finish");
              }));

      sub.Request(ElementCount(1));
      sub.Request(ElementCount(1));
    }
  }

  SECTION("stream passed by lvalue") {
    auto inner_stream = Just();
    auto stream = Zip<std::tuple<int>>(inner_stream);
    CHECK(GetAll<std::tuple<int>>(stream) == (std::vector<std::tuple<int>>{}));
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(Zip<std::tuple<int>>(Just(1)));
  }

  SECTION("cancellation") {
    auto fail = std::make_exception_ptr(std::runtime_error("test_fail"));

    SECTION("request elements after cancellation") {
      auto stream = Zip<std::tuple<int, int>>(
          Concat(Just(1), Just(3), Throw(fail)),
          Concat(Just(2, 4), Throw(fail)));

      std::vector<std::tuple<int, int>> result;
      bool is_done = false;
      auto sub = stream.Subscribe(MakeSubscriber(
          [&is_done, &result](auto val) {
            CHECK(!is_done);
            result.push_back(val);
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [&is_done] {
            CHECK(!is_done);
            is_done = true;
          }));

      sub.Request(ElementCount(1));
      CHECK(
          result ==
          (std::vector<std::tuple<int, int>>{ std::make_tuple(1, 2) }));
      sub.Cancel();
      sub.Request(ElementCount(1));
      CHECK(
          result ==
          (std::vector<std::tuple<int, int>>{ std::make_tuple(1, 2) }));
    }

    SECTION("emit elements after cancellation") {
      std::function<void ()> emit;
      auto stream = Zip<std::tuple<int>>(MakePublisher([&emit](
          auto subscriber) {
        auto subscriber_ptr = std::make_shared<decltype(subscriber)>(
            std::move(subscriber));
        emit = [subscriber_ptr] {
          subscriber_ptr->OnNext(1);
        };
        return MakeSubscription();
      }));

      std::vector<std::tuple<int>> result;
      bool is_done = false;
      auto sub = stream.Subscribe(MakeSubscriber(
          [&is_done, &result](auto val) {
            CHECK(!is_done);
            result.push_back(val);
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [&is_done] {
            CHECK(!is_done);
            is_done = true;
          }));

      sub.Request(ElementCount(1));
      sub.Cancel();
      emit();
      CHECK(result == std::vector<std::tuple<int>>());
    }

    SECTION("Cancel cancels underlying subscriptions") {
      bool cancelled = false;
      auto stream = Zip<std::tuple<int>>(MakePublisher([&cancelled](auto) {
        return MakeSubscription(
            [](ElementCount) {},
            [&cancelled] {
              cancelled = true;
            });
      }));

      auto sub = stream.Subscribe(MakeSubscriber(
          [](auto val) {
            CHECK(!"OnNext should not be called");
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [] {
            CHECK(!"OnComplete should not be called");
          }));

      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }
  }

  SECTION("exceptions") {
    auto fail = std::make_exception_ptr(std::runtime_error("test_fail"));

    SECTION("one failing stream") {
      auto stream = Zip<std::tuple<int>>(Throw(fail));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("one failing stream, the other infinite") {
      auto stream = Zip<std::tuple<int, int>>(Throw(fail), InfiniteRange(0));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("one failing stream but don't request to the error") {
      auto stream = Zip<std::tuple<int>>(Concat(Just(1, 2), Throw(fail)));
      CHECK(
          GetAll<std::tuple<int>>(stream, ElementCount(1), false) ==
          (std::vector<std::tuple<int>>{ 1 }));
    }

    SECTION("two failing streams but don't request to the error") {
      auto stream = Zip<std::tuple<int, int>>(
          Concat(Just(1), Just(3), Throw(fail)),
          Concat(Just(2, 4), Throw(fail)));

      std::vector<std::tuple<int, int>> result;
      bool is_done = false;
      auto sub = stream.Subscribe(MakeSubscriber(
          [&is_done, &result](auto &&val) {
            CHECK(!is_done);
            result.emplace_back(std::forward<decltype(val)>(val));
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [&is_done] {
            CHECK(!is_done);
            is_done = true;
          }));

      sub.Request(ElementCount(1));
      CHECK(!is_done);
      CHECK(
          result ==
          (std::vector<std::tuple<int, int>>{ std::make_tuple(1, 2) }));
    }

    SECTION("one failing one succeeding stream") {
      auto stream = Zip<std::tuple<int, int>>(Throw(fail), Just(1));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("one succeeding one failing stream") {
      auto stream = Zip<std::tuple<int, int>>(Just(1), Throw(fail));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("two failing streams") {
      auto stream = Zip<std::tuple<int, int>>(Throw(fail), Throw(fail));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }
  }

  SECTION("backpressure violation") {
    SECTION("one too much") {
      auto stream = Zip<std::tuple<int>>(
          BackpressureViolator(1, [] { return 0; }));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "Backpressure violation");
    }

    SECTION("two too much") {
      auto stream = Zip<std::tuple<int>>(
          BackpressureViolator(2, [] { return 0; }));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "Backpressure violation");
    }
  }
}

}  // namespace shk
