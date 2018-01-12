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

#include <rs/catch.h>
#include <rs/empty.h>
#include <rs/just.h>
#include <rs/pipe.h>
#include <rs/start_with.h>
#include <rs/throw.h>

#include "backpressure_violator.h"
#include "test_util.h"

namespace shk {

TEST_CASE("Catch") {
  auto null_catch = Catch([](std::exception_ptr &&) {
    CHECK(!"should not be called");
    return Empty();
  });
  auto empty_catch = Catch([](std::exception_ptr &&) { return Empty(); });
  auto single_catch = Catch([](std::exception_ptr &&) { return Just(14); });
  auto failing = Throw(std::runtime_error("test"));
  auto failing_catch = Catch([failing](std::exception_ptr &&) {
    return failing;
  });
  auto failing_later = Pipe(
      Throw(std::runtime_error("test")),
      StartWith(13));
  static_assert(
      IsPublisher<decltype(empty_catch(Just()))>,
      "Catch stream should be a publisher");

  SECTION("subscription is default constructible") {
    SECTION("as returned from Catch") {
      auto stream = null_catch(Just());
      decltype(stream.Subscribe(MakeNonDefaultConstructibleSubscriber())) sub;
      sub.Request(ElementCount(1));
      sub.Cancel();
    }

    SECTION("CatchSubscription") {
      auto subscriber = MakeNonDefaultConstructibleSubscriber();
      auto callback = [](auto) { return Empty(); };
      using CatchT = detail::Catch<
          decltype(subscriber),
          decltype(callback),
          AnySubscription>;

      typename CatchT::CatchSubscription sub;
      sub.Request(ElementCount(1));
      sub.Cancel();
    }
  }

  SECTION("succeeding input") {
    SECTION("empty") {
      CHECK(GetAll<int>(null_catch(Just())) == std::vector<int>({}));
    }

    SECTION("one value") {
      CHECK(GetAll<int>(null_catch(Just(1))) == std::vector<int>({ 1 }));
    }
  }

  SECTION("failing input") {
    SECTION("empty catch") {
      SECTION("empty input") {
        CHECK(GetAll<int>(empty_catch(failing)) == std::vector<int>({}));
      }

      SECTION("one input value") {
        CHECK(
            GetAll<int>(empty_catch(failing_later)) ==
            std::vector<int>({ 13 }));
      }
    }

    SECTION("nonempty catch") {
      SECTION("empty input") {
        CHECK(GetAll<int>(single_catch(failing)) == std::vector<int>({ 14 }));
      }

      SECTION("one input value") {
        CHECK(
            GetAll<int>(single_catch(failing_later)) ==
            std::vector<int>({ 13, 14 }));
      }

      SECTION("one input value, request 0") {
        CHECK(
            GetAll<int>(single_catch(failing_later), ElementCount(0), false) ==
            std::vector<int>({}));
      }

      SECTION("one input value, request 1") {
        CHECK(
            GetAll<int>(single_catch(failing_later), ElementCount(1), false) ==
            std::vector<int>({ 13 }));
      }

      SECTION("one input value, request 2") {
        CHECK(
            GetAll<int>(single_catch(failing_later), ElementCount(2)) ==
            std::vector<int>({ 13, 14 }));
      }

      SECTION("one input value, request 3") {
        CHECK(
            GetAll<int>(single_catch(failing_later), ElementCount(3)) ==
            std::vector<int>({ 13, 14 }));
      }
    }

    SECTION("failing catch") {
      SECTION("empty input") {
        auto stream = failing_catch(failing);

        auto error = GetError(stream);
        CHECK(GetErrorWhat(error) == "test");

        CHECK(GetAll<int>(empty_catch(stream)) == std::vector<int>({}));
      }

      SECTION("one input value") {
        auto stream = failing_catch(failing_later);

        auto error = GetError(stream);
        CHECK(GetErrorWhat(error) == "test");

        CHECK(GetAll<int>(empty_catch(stream)) == std::vector<int>({ 13 }));
      }
    }
  }

  SECTION("don't leak the subscriber") {
    CheckLeak(empty_catch(Just(1)));
    CheckLeak(empty_catch(failing));
  }

  SECTION("cancellation") {
    SECTION("cancel before failing") {
      bool cancelled = false;
      auto stream = null_catch(MakePublisher([&cancelled](auto &&subscriber) {
        return MakeSubscription(
            [](ElementCount count) {},
            [&cancelled] { CHECK(!cancelled); cancelled = true; });
      }));

      auto sub = stream.Subscribe(MakeSubscriber());
      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }

    SECTION("cancel after failing") {
      bool cancelled = false;
      auto check_catch = Catch([&cancelled](std::exception_ptr &&) {
        return MakePublisher([&cancelled](auto &&subscriber) {
          return MakeSubscription(
              [](ElementCount count) {},
              [&cancelled] { CHECK(!cancelled); cancelled = true; });
        });
      });
      auto stream = check_catch(Throw(std::runtime_error("test")));

      auto sub = stream.Subscribe(MakeSubscriber());
      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }

    SECTION("do not invoke catch if fail after cancel") {
      std::function<void ()> fail;
      auto fail_on_demand = MakePublisher([&fail](auto subscriber) {
        auto subscriber_ptr = std::make_shared<decltype(subscriber)>(
            std::move(subscriber));

        fail = [subscriber_ptr]() mutable {
          subscriber_ptr->OnError(
              std::make_exception_ptr(std::runtime_error("test")));
        };
        return MakeSubscription();
      });

      auto stream = null_catch(fail_on_demand);

      auto sub = stream.Subscribe(MakeSubscriber());
      sub.Cancel();
      fail();  // This should not invoke null_catch's callback
    }
  }

  SECTION("backpressure violation") {
    auto stream = null_catch(BackpressureViolator(1, [] { return 0; }));
    auto error = GetError(stream, ElementCount(1));
    CHECK(GetErrorWhat(error) == "Got value that was not Request-ed");
  }
}

}  // namespace shk
