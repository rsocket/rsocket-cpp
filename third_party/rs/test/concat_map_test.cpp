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

#include <rs/element_count.h>
#include <rs/empty.h>
#include <rs/concat_map.h>
#include <rs/from.h>
#include <rs/just.h>
#include <rs/map.h>
#include <rs/never.h>
#include <rs/pipe.h>

#include "backpressure_violator.h"
#include "infinite_range.h"
#include "test_util.h"

namespace shk {

TEST_CASE("ConcatMap") {
  SECTION("construct") {
    auto stream = ConcatMap([](auto &&) { return Empty(); })(Empty());
    static_assert(
        IsPublisher<decltype(stream)>,
        "ConcatMap stream should be a publisher");
  }

  SECTION("subscription is default constructible") {
    auto stream = ConcatMap([](auto &&) { return Empty(); })(Empty());
    decltype(stream.Subscribe(MakeNonDefaultConstructibleSubscriber())) sub;
    sub.Request(ElementCount(1));
    sub.Cancel();
  }

  SECTION("no streams") {
    auto stream = ConcatMap([](auto &&) { return Empty(); })(Empty());
    CHECK(GetAll<int>(stream) == std::vector<int>({}));
  }

  SECTION("one empty stream") {
    auto stream = ConcatMap([](auto &&) { return Empty(); })(Just(1));
    CHECK(GetAll<int>(stream) == std::vector<int>({}));
  }

  SECTION("one empty stream, request 0") {
    auto concat_map = ConcatMap([](auto &&) { return Just(1); });
    auto stream = concat_map(Start([] {
      return 0;
    }));
    CHECK(GetAll<int>(stream, ElementCount(0), false) == std::vector<int>({}));
  }

  SECTION("one stream with one value") {
    auto stream = ConcatMap([](auto &&) { return Just(2); })(Just(1));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 2 }));
  }

  SECTION("one stream with two values") {
    auto concat_map = ConcatMap([](auto &&) {
      return From(std::vector<int>{ 1, 2 });
    });
    auto stream = concat_map(Just(1));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 1, 2 }));
  }

  SECTION("two streams with one value") {
    auto stream = ConcatMap([](auto &&) { return Just(2); })(
        From(std::vector<int>{ 0, 0 }));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 2, 2 }));
  }

  SECTION("two streams with two values") {
    auto concat_map = ConcatMap([](auto &&) {
      return From(std::vector<int>{ 1, 2 });
    });
    auto stream = concat_map(From(std::vector<int>{ 0, 0 }));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 1, 2, 1, 2 }));
  }

  SECTION("requesting parts of inner stream at a time") {
    for (int i = 1; i <= 2; i++) {
      // Depending on if we're requesting in the last stream or not,
      // ConcatMapSubscriber::Request will be in state_ == HAS_PUBLISHER or
      // state_ == ON_LAST_PUBLISHER. With this loop we test both.

      auto concat_map = ConcatMap([](auto &&) {
        return From(std::vector<int>{ 1, 2, 3, 4 });
      });
      auto stream = concat_map(From(std::vector<int>(i, 0)));

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
      for (int j = 0; j < i; j++) {
        sub.Request(ElementCount(2));
        sub.Request(ElementCount(2));
      }
      CHECK(is_done);
      if (i == 1) {
        CHECK(result == std::vector<int>({ 1, 2, 3, 4 }));
      } else {
        CHECK(result == std::vector<int>({ 1, 2, 3, 4, 1, 2, 3, 4 }));
      }
    }
  }

  SECTION("requesting from within OnNext") {
    auto concat_map = ConcatMap([](auto &&) {
      return From(std::vector<int>{ 1, 2, 3 });
    });
    auto stream = concat_map(Just(1, 2));

    std::vector<int> result;
    bool is_done = false;
    AnySubscription sub = AnySubscription(stream.Subscribe(MakeSubscriber(
        [&is_done, &result, &sub](auto &&val) {
          CHECK(!is_done);
          result.emplace_back(std::forward<decltype(val)>(val));
          sub.Request(ElementCount(1));
        },
        [](std::exception_ptr &&error) {
          CHECK(!"OnError should not be called");
        },
        [&is_done] {
          CHECK(!is_done);
          is_done = true;
        })));
    sub.Request(ElementCount(1));
    CHECK(is_done);
    CHECK(result == std::vector<int>({ 1, 2, 3, 1, 2, 3 }));
  }

  SECTION("get first and only value asynchronously") {
    bool subscribed = false;
    std::function<void (int)> on_next;
    auto inner_stream = MakePublisher([&subscribed, &on_next](
        auto subscriber) {
      CHECK(!subscribed);
      subscribed = true;

      auto shared_subscriber = std::make_shared<decltype(subscriber)>(
          std::move(subscriber));
      on_next = [shared_subscriber](int value) mutable {
        shared_subscriber->OnNext(int(value));
        shared_subscriber->OnComplete();
      };

      return MakeSubscription();
    });

    auto concat_map = ConcatMap([inner_stream = std::move(inner_stream)](
        auto x) {
      return inner_stream;
    });

    auto stream = concat_map(Just(0));
    bool next_called = false;
    bool complete_called = false;
    auto sub = stream.Subscribe(MakeSubscriber(
        [&next_called, &complete_called](int val) {
          CHECK(val == 123);
          CHECK(!complete_called);
          CHECK(!next_called);
          next_called = true;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&complete_called, &next_called] {
          CHECK(next_called);
          CHECK(!complete_called);
          complete_called = true;
        }));

    CHECK(subscribed);
    sub.Request(ElementCount(1));
    CHECK(subscribed);
    CHECK(!next_called);
    CHECK(!complete_called);

    // OnComplete should not be called until this
    on_next(123);
    CHECK(next_called);
    CHECK(complete_called);
  }

  SECTION("ConcatMap of three") {
    // This has caused use-after-free errors
    auto concat_map = ConcatMap([](auto &&publisher) { return publisher; });
    auto stream = concat_map(Just(Just(1), Just(2), Empty()));
    CHECK(GetAll<int>(stream) == std::vector<int>({ 1, 2 }));
  }

  SECTION("don't leak the subscriber") {
    auto stream = Pipe(
        Just(1),
        ConcatMap([](auto &&) {
          return From(std::vector<int>{ 1, 2 });
        }));
    CheckLeak(stream);
  }

  SECTION("backpressure violation") {
    SECTION("outer stream") {
      auto violator = BackpressureViolator(2, [] { return 0; });
      auto stream = ConcatMap([](auto &&) { return Just(1); })(violator);
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "Got value that was not Request-ed");
    }

    SECTION("outer stream multiple violations") {
      // Should still only fail once

      auto violator = BackpressureViolator(3, [] { return 0; });
      auto stream = ConcatMap([](auto &&) { return Just(1); })(violator);
      auto error = GetError(stream);
      CHECK(GetErrorWhat(error) == "Got value that was not Request-ed");
    }

    SECTION("should not subscribe to publisher after backpressure violation") {
      std::exception_ptr error;

      auto stream = Pipe(
          BackpressureViolator(3, [] { return 0; }),
          ConcatMap([&error](auto &&) {
            return MakePublisher([&error](auto &&subscriber) {
              CHECK(!error);
              return MakeSubscription();
            });
          }));

      auto sub = stream.Subscribe(MakeSubscriber(
          [&error](auto &&next) {
            CHECK(!error);
          },
          [&error](std::exception_ptr &&received_error) {
            CHECK(!error);
            error = received_error;
          },
          [] { CHECK(!"OnComplete should not be called"); }));
      sub.Request(ElementCount::Unbounded());
      CHECK(error);
      CHECK(GetErrorWhat(error) == "Got value that was not Request-ed");
    }

    SECTION("inner stream") {
      auto violator = BackpressureViolator(1, [] { return 0; });
      auto stream =
          ConcatMap([&violator](auto &&) { return violator; })(Just(1));
      auto error = GetError(stream, ElementCount(1));
      CHECK(GetErrorWhat(error) == "Got value that was not Request-ed");
    }

    SECTION("two inner streams") {
      auto violator = BackpressureViolator(1, [] { return 0; });
      auto stream = Pipe(
          Just(1),
          ConcatMap([&violator](int value) {
            CHECK(value != 2);
            return violator;
          }));
      auto error = GetError(stream, ElementCount(1));
      CHECK(GetErrorWhat(error) == "Got value that was not Request-ed");
    }
  }

  SECTION("cancellation") {
    SECTION("publisher stream") {
      bool cancelled = false;
      auto inner_stream = MakePublisher([&cancelled](auto subscriber) {
        return MakeSubscription(
          [called = false](ElementCount count) mutable {
            CHECK(!called);
            called = true;
            CHECK(count == 1);
          },
          [&cancelled] {
            CHECK(!cancelled);
            cancelled = true;
          });
      });

      auto concat_map = ConcatMap([](auto x) { return Never(); });

      auto stream = concat_map(inner_stream);
      auto sub = stream.Subscribe(MakeSubscriber(
          [](auto) { CHECK(!"OnNext should not be called"); },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [] { CHECK(!"OnComplete should not be called"); }));

      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }

    SECTION("values stream") {
      bool cancelled = false;
      auto inner_stream = MakePublisher([&cancelled](auto subscriber) {
        return MakeSubscription(
          [&cancelled](ElementCount count) {
            CHECK(!cancelled);
          },
          [&cancelled] {
            CHECK(!cancelled);
            cancelled = true;
          });
      });

      auto concat_map =
          ConcatMap([&inner_stream](auto x) { return inner_stream; });

      auto stream = concat_map(Just(0));
      auto sub = stream.Subscribe(MakeSubscriber(
          [](auto) { CHECK(!"OnNext should not be called"); },
          [](std::exception_ptr &&error) {
            CHECK(!"OnError should not be called");
          },
          [] { CHECK(!"OnComplete should not be called"); }));

      sub.Request(ElementCount(1));
      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }
  }

  SECTION("exceptions in input stream") {
    auto fail_on = [](int error_val) {
      return ConcatMap([error_val](int x) {
        if (x == error_val) {
          throw std::runtime_error("fail_on");
        } else {
          return Just(42);
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

    SECTION("error on second only one requested") {
      auto error = GetError(
          fail_on(0)(From(std::vector<int>{ 0, 0 })),
          ElementCount(1));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("error on third only one requested") {
      CHECK(
          GetAll<int>(
              fail_on(0)(From(std::vector<int>{ 1, 1, 0 })),
              ElementCount(1),
              false) ==
          (std::vector<int>{ 42 }));
    }

    SECTION("error on first of infinite") {
      // This will terminate only if the Map operator actually cancels the
      // underlying InfiniteRange stream.
      auto error = GetError(fail_on(0)(InfiniteRange(0)));
      CHECK(GetErrorWhat(error) == "fail_on");
    }

    SECTION("source emits value that fails and then fails itself") {
      auto zero_then_fail = fail_on(1)(From(std::vector<int>{ 0, 1 }));

      // Should only fail once. GetError checks that
      auto error = GetError(fail_on(42)(zero_then_fail));
      CHECK(GetErrorWhat(error) == "fail_on");
    }
  }

  SECTION("exceptions in stream returned from mapper") {
    auto fail_after = [](int delay) {
      std::vector<int> vec;
      for (int i = 0; i < (delay < 0 ? -delay : delay); i++) {
        vec.push_back(0);
      }

      if (delay != -1) {
        vec.push_back(1);
      }

      return Map([](int value) {
        if (value == 1) {
          throw std::runtime_error("fail_after");
        } else {
          return value;
        }
      })(From(std::vector<int>(vec)));
    };

    auto fail_on_inner = [&fail_after]() {
      return ConcatMap([&fail_after](int x) {
        return fail_after(x);
      });
    };

    SECTION("immediate error on first") {
      auto error = GetError(fail_on_inner()(From(std::vector<int>{ 0 })));
      CHECK(GetErrorWhat(error) == "fail_after");
    }

    SECTION("delayed error on first") {
      auto error = GetError(fail_on_inner()(From(std::vector<int>{ 1 })));
      CHECK(GetErrorWhat(error) == "fail_after");
    }

    SECTION("immediate error on second") {
      auto error = GetError(
          fail_on_inner()(From(std::vector<int>{ -1, 0 })));
      CHECK(GetErrorWhat(error) == "fail_after");
    }

    SECTION("delayed error on second") {
      auto error = GetError(
          fail_on_inner()(From(std::vector<int>{ -1, 1 })));
      CHECK(GetErrorWhat(error) == "fail_after");
    }

    SECTION("error on first and second") {
      auto error = GetError(
          fail_on_inner()(From(std::vector<int>{ 0, 0 })));
      CHECK(GetErrorWhat(error) == "fail_after");
    }

    SECTION("error on second only one requested") {
      CHECK(
          GetAll<int>(
              fail_on_inner()(From(std::vector<int>{ -1, 0 })),
              ElementCount(1),
              false) ==
          (std::vector<int>{ 0 }));
    }

    SECTION("delayed error on first only one requested") {
      CHECK(
          GetAll<int>(
              fail_on_inner()(From(std::vector<int>{ 1, 0 })),
              ElementCount(1),
              false) ==
          (std::vector<int>{ 0 }));
    }

    SECTION("immediate error on first of infinite") {
      // This will terminate only if the Map operator actually cancels the
      // underlying InfiniteRange stream.
      auto error = GetError(fail_on_inner()(InfiniteRange(0)));
      CHECK(GetErrorWhat(error) == "fail_after");
    }

    SECTION("delayed error on first of infinite") {
      // This will terminate only if the Map operator actually cancels the
      // underlying InfiniteRange stream.
      auto error = GetError(fail_on_inner()(InfiniteRange(1)));
      CHECK(GetErrorWhat(error) == "fail_after");
    }
  }
}

}  // namespace shk
