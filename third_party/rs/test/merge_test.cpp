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
#include <rs/just.h>
#include <rs/merge.h>
#include <rs/range.h>
#include <rs/throw.h>

#include "backpressure_violator.h"
#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Merge") {
  SECTION("type") {
    static_assert(
        IsPublisher<decltype(Merge<int>())>,
        "Merge stream should be a publisher");
  }

  SECTION("subscription is default constructible") {
    SECTION("as returned from Merge") {
      auto stream = Merge<int>(Just());
      decltype(stream.Subscribe(MakeNonDefaultConstructibleSubscriber())) sub;
      sub.Request(ElementCount(1));
      sub.Cancel();
    }

    SECTION("MergeSubscription") {
      auto subscriber = MakeNonDefaultConstructibleSubscriber();
      detail::MergeSubscription<decltype(subscriber), int> sub;
      sub.Request(ElementCount(1));
      sub.Cancel();
    }
  }

  SECTION("stream passed by lvalue") {
    auto inner_stream = Just();
    auto stream = Merge<int>(inner_stream);
    CHECK(GetAll<int>(stream) == (std::vector<int>{}));
  }

  SECTION("no streams") {
    SECTION("output") {
      auto stream = Merge<int>();
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("should instantly complete") {
      auto stream = Merge<int>();

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

  SECTION("backpressure violation") {
    SECTION("during Subscribe call") {
      auto stream = Merge<int>(BackpressureViolator(1, [] { return 0; }));
      auto error = GetError(stream, ElementCount(1));
      CHECK(GetErrorWhat(error) == "Got value before Requesting anything");
    }

    SECTION("after Subscribe call") {
      std::function<void ()> emit;
      auto stream = Merge<int>(MakePublisher([&emit](auto subscriber) {
        auto subscriber_ptr = std::make_shared<decltype(subscriber)>(
            std::move(subscriber));

        emit = [subscriber_ptr] {
          subscriber_ptr->OnNext(1);
        };
        return MakeSubscription();
      }));

      std::exception_ptr received_error;

      auto sub = stream.Subscribe(MakeSubscriber(
          [&received_error](int next) {
            CHECK(!"OnNext should not be called");
          },
          [&received_error](std::exception_ptr &&error) {
            CHECK(!received_error);
            received_error = error;
          },
          [] { CHECK(!"OnComplete should not be called"); }));

      REQUIRE(emit);
      emit();

      CHECK(
          GetErrorWhat(received_error) ==
          "Got value that was not Request-ed");
    }
  }

  SECTION("one empty stream") {
    SECTION("output") {
      auto stream = Merge<int>(Just());
      CHECK(GetAll<int>(stream) == (std::vector<int>{}));
    }

    SECTION("should instantly complete") {
      auto stream = Merge<int>(Just());

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

  SECTION("request from within OnNext") {
    int nexts = 0;
    int finishes = 0;

    // Merge two streams and request 10 from them, to trigger buffering, which
    // is the case when this can fail.
    auto stream = Merge<int>(Range(0, 10), Range(0, 10));

    bool finished = false;
    bool in_on_next = false;
    bool request_more = false;
    AnySubscription sub = AnySubscription(stream.Subscribe(MakeSubscriber(
        [&in_on_next, &request_more, &sub](int next) {
          CHECK(!in_on_next);
          in_on_next = true;
          // If Merge does this wrong, it could call OnNext from within Request
          // here.
          if (request_more) {
            sub.Request(ElementCount(1));
          }
          in_on_next = false;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&finished] {
          CHECK(!finished);
          finished = true;
        })));

    CHECK(!finished);
    sub.Request(ElementCount(10));
    request_more = true;
    sub.Request(ElementCount(1));
    CHECK(finished);
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(Merge<int>(Just(1), Just(2)));
  }

  SECTION("cancellation") {
    auto fail = std::make_exception_ptr(std::runtime_error("test_fail"));

    SECTION("request elements after cancellation") {
      auto stream = Merge<int>(
          Concat(Just(1), Just(3), Throw(fail)),
          Concat(Just(2), Just(4), Throw(fail)));

      std::vector<int> result;
      bool is_done = false;
      auto sub = stream.Subscribe(MakeSubscriber(
          [&is_done, &result](int val) {
            CHECK(!is_done);
            result.emplace_back(val);
          },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [&is_done] {
            CHECK(!is_done);
            is_done = true;
          }));

      sub.Request(ElementCount(1));
      CHECK(result == std::vector<int>({ 1 }));
      sub.Cancel();
      sub.Request(ElementCount(1));
      CHECK(result == std::vector<int>({ 1 }));
    }

    SECTION("emit elements after cancellation") {
      std::function<void ()> emit;
      auto stream = Merge<int>(MakePublisher([&emit](auto subscriber) {
        auto subscriber_ptr = std::make_shared<decltype(subscriber)>(
            std::move(subscriber));

        emit = [subscriber_ptr] {
          subscriber_ptr->OnNext(1);
        };
        return MakeSubscription();
      }));

      std::vector<int> result;
      bool is_done = false;
      auto sub = stream.Subscribe(MakeSubscriber(
          [&is_done, &result](int val) {
            CHECK(!is_done);
            result.emplace_back(val);
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
      CHECK(result == std::vector<int>());
    }

    SECTION("Cancel cancels underlying subscriptions") {
      bool cancelled = false;
      auto stream = Merge<int>(MakePublisher([&cancelled](auto subscriber) {
        return MakeSubscription(
            [](ElementCount) {},
            [&cancelled] {
              cancelled = true;
            });
      }));

      auto sub = stream.Subscribe(MakeSubscriber(
          [](int val) {
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

  SECTION("two empty streams") {
    auto stream = Merge<int>(Just(), Just());
    CHECK(GetAll<int>(stream) == (std::vector<int>{}));
  }

  SECTION("one stream with one value") {
    auto stream = Merge<int>(Just(1));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1 }));
  }

  SECTION("one stream with two values") {
    auto stream = Merge<int>(Just(1, 2));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2 }));
  }

  SECTION("two streams with one value") {
    auto stream = Merge<int>(Just(1), Just(2));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2 }));
  }

  SECTION("two streams with two values") {
    auto stream = Merge<int>(Just(1, 2), Just(3, 4));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3, 4 }));
  }

  SECTION("two streams where first is longer") {
    auto stream = Merge<int>(Just(1, 2), Just(3));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
  }

  SECTION("two streams where second is longer") {
    auto stream = Merge<int>(Just(1), Just(2, 3));
    CHECK(GetAll<int>(stream) == (std::vector<int>{ 1, 2, 3 }));
  }

  SECTION("two streams with two values, request two") {
    auto stream = Merge<int>(Just(1, 2), Just(3, 4));
    CHECK(
        GetAll<int>(stream, ElementCount(2), false) ==
        (std::vector<int>{ 1, 2 }));
  }

  SECTION("requesting parts of stream at a time") {
    for (int i = 1; i <= 5; i++) {
      auto stream = Merge<int>(Just(1, 2), Just(3, 4));

      std::vector<int> result;
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
      if (i == 1) {
        CHECK(result == std::vector<int>({ 1, 3, 2, 4 }));
      } else {
        CHECK(result == std::vector<int>({ 1, 2, 3, 4 }));
      }
    }
  }

  SECTION("exceptions") {
    auto fail = std::make_exception_ptr(std::runtime_error("test_fail"));

    SECTION("one failing stream") {
      auto stream = Merge<int>(Throw(fail));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("one failing stream, the other infinite") {
      auto stream = Merge<int>(Throw(fail), InfiniteRange(0));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("one failing stream but don't request to the error") {
      auto stream = Merge<int>(Concat(Just(1, 2), Throw(fail)));
      CHECK(
          GetAll<int>(stream, ElementCount(1), false) ==
          (std::vector<int>{ 1 }));
    }

    SECTION("two failing streams but don't request to the error") {
      auto stream = Merge<int>(
          Concat(Just(1), Just(3), Throw(fail)),
          Concat(Just(2, 4), Throw(fail)));

      std::vector<int> result;
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
      sub.Request(ElementCount(1));
      CHECK(!is_done);
      CHECK(result == std::vector<int>({ 1, 2 }));
    }

    SECTION("one failing one succeeding stream") {
      auto stream = Merge<int>(Throw(fail), Just(1));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("one succeeding one failing stream") {
      auto stream = Merge<int>(Just(1), Throw(fail));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }

    SECTION("two failing streams") {
      auto stream = Merge<int>(Throw(fail), Throw(fail));
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "test_fail");
    }
  }
}

}  // namespace shk
