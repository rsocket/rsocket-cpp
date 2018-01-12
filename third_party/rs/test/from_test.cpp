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

#include <rs/from.h>
#include <rs/subscriber.h>

#include "infinite_range.h"
#include "test_util.h"

namespace shk {
namespace {

class DummyContainer {
 public:
  template <typename Value>
  class Iterator {
   public:
    Iterator() {}

    Value &operator*() {
      return v_;
    }

    const Value &operator*() const {
      return v_;
    }

    Iterator &operator++() {
      return *this;
    }

    bool operator==(const Iterator &other) {
      return true;
    }

   private:
    Value v_;
  };

  Iterator<std::string> begin() {
    return Iterator<std::string>();
  }

  Iterator<const std::string> begin() const {
    return Iterator<const std::string>();
  }

  Iterator<std::string> end() {
    return Iterator<std::string>();
  }

  Iterator<const std::string> end() const {
    return Iterator<const std::string>();
  }
};

}  // anonymous namespace

TEST_CASE("From") {
  SECTION("construct") {
    auto stream = From(std::vector<int>{});
    static_assert(
        IsPublisher<decltype(stream)>,
        "From should be a publisher");
  }

  SECTION("subscription is default constructible") {
    auto stream = From(std::vector<int>{});
    decltype(stream.Subscribe(MakeNonDefaultConstructibleSubscriber())) sub;
    sub.Request(ElementCount(1));
    sub.Cancel();
  }

  SECTION("empty container") {
    auto stream = From(std::vector<int>{});

    int done = 0;
    auto sub = stream.Subscribe(MakeSubscriber(
        [](int next) { CHECK(!"should not happen"); },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done] { done++; }));
    CHECK(done == 1);
    sub.Request(ElementCount(1));
    CHECK(done == 1);
  }

  SECTION("lvalue ref container") {
    std::vector<int> empty;
    auto stream = From(empty);

    int done = 0;
    auto sub = stream.Subscribe(MakeSubscriber(
        [](int next) { CHECK(!"should not happen"); },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done] { done++; }));
    CHECK(done == 1);
    sub.Request(ElementCount(1));
    CHECK(done == 1);
  }

  SECTION("one value") {
    auto stream = From(std::vector<int>{ 1 });

    int done = 0;
    int next = 0;
    auto sub = stream.Subscribe(MakeSubscriber(
        [&next](int val) {
          CHECK(val == 1);
          next++;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done] {
          done++;
        }));
    CHECK(done == 0);
    CHECK(next == 0);
    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 1);
    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 1);
  }

  SECTION("container with const and non-const accessor") {
    // Make sure it doesn't mix up const and non-const iterator types
    From(DummyContainer()).Subscribe(MakeSubscriber());
  }

  SECTION("move Subscription with string container") {
    // Short std::string objects (that don't heap allocate) will invalidate
    // all iterators to it when moving the string. In order to support this,
    // From must make sure to not move the container when moving the
    // subscription (because it keeps iterators in the subscription).
    auto stream = From(std::string("a"));

    int done = 0;
    int next = 0;
    auto sub_pre_move = stream.Subscribe(MakeSubscriber(
        [&next](char val) {
          CHECK(val == 'a');
          next++;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done] {
          done++;
        }));
    auto sub = std::move(sub_pre_move);
    CHECK(done == 0);
    CHECK(next == 0);
    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 1);
    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 1);
  }

  SECTION("multiple values, one at a time") {
    auto stream = From(std::vector<int>{ 1, 2 });

    int done = 0;
    int next = 0;
    auto sub = stream.Subscribe(MakeSubscriber(
        [&next](int val) {
          if (next == 0) {
            CHECK(val == 1);
          } else if (next == 1) {
            CHECK(val == 2);
          } else {
            CHECK(!"got too many values");
          }
          next++;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done, &next] {
          CHECK(done == 0);
          CHECK(next == 2);
          done++;
        }));
    CHECK(done == 0);
    CHECK(next == 0);

    sub.Request(ElementCount(1));
    CHECK(done == 0);
    CHECK(next == 1);

    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 2);

    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 2);
  }

  SECTION("multiple values, all at once") {
    auto stream = From(std::vector<int>{ 1, 2 });

    int done = 0;
    int next = 0;
    auto sub = stream.Subscribe(MakeSubscriber(
        [&next](int val) {
          if (next == 0) {
            CHECK(val == 1);
          } else if (next == 1) {
            CHECK(val == 2);
          } else {
            CHECK(!"got too many values");
          }
          next++;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done, &next] {
          CHECK(done == 0);
          CHECK(next == 2);
          done++;
        }));
    CHECK(done == 0);
    CHECK(next == 0);

    sub.Request(ElementCount(2));
    CHECK(done == 1);
    CHECK(next == 2);

    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 2);
  }

  SECTION("multiple values, more than all at once") {
    auto stream = From(std::vector<int>{ 1, 2 });

    int done = 0;
    int next = 0;
    auto sub = stream.Subscribe(MakeSubscriber(
        [&next](int val) {
          if (next == 0) {
            CHECK(val == 1);
          } else if (next == 1) {
            CHECK(val == 2);
          } else {
            CHECK(!"got too many values");
          }
          next++;
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [&done, &next] {
          CHECK(done == 0);
          CHECK(next == 2);
          done++;
        }));
    CHECK(done == 0);
    CHECK(next == 0);

    sub.Request(ElementCount::Unbounded());
    CHECK(done == 1);
    CHECK(next == 2);

    sub.Request(ElementCount(1));
    CHECK(done == 1);
    CHECK(next == 2);
  }

  SECTION("multiple iterations") {
    auto stream = From(std::vector<int>{ 1 });

    for (int i = 0; i < 3; i++) {
      int done = 0;
      int next = 0;
      auto sub = stream.Subscribe(MakeSubscriber(
          [&next](int val) {
            CHECK(next == 0);
            CHECK(val == 1);
            next++;
          },
          [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
          [&done, &next] {
            CHECK(done == 0);
            CHECK(next == 1);
            done++;
          }));

      CHECK(done == 0);
      sub.Request(ElementCount(1));
      CHECK(done == 1);
      sub.Request(ElementCount(1));
    }
  }

  SECTION("request from within OnNext") {
    SECTION("one value") {
      int nexts = 0;
      int finishes = 0;

      auto stream = From(std::vector<int>{ 1 });

      AnySubscription sub = AnySubscription(stream.Subscribe(MakeSubscriber(
          [&sub, &nexts](int next) {
            CHECK(nexts == 0);
            CHECK(next == 1);
            nexts++;
            // If Start does this wrong, it will blow the stack
            sub.Request(ElementCount(1));
          },
          [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
          [&finishes, &nexts] {
            CHECK(nexts == 1);
            finishes++;
          })));
      CHECK(nexts == 0);
      CHECK(finishes == 0);

      sub.Request(ElementCount(1));
      CHECK(nexts == 1);
      CHECK(finishes == 1);
    }

    SECTION("two values") {
      int nexts = 0;
      int finishes = 0;

      auto stream = From(std::vector<int>{ 1, 2 });

      AnySubscription sub = AnySubscription(stream.Subscribe(MakeSubscriber(
          [&sub, &nexts](int next) {
            CHECK(nexts < 2);
            nexts++;
            CHECK(next == nexts);
            // If Start does this wrong, it will blow the stack, or result in
            // two calls to OnComplete
            sub.Request(ElementCount(1));
          },
          [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
          [&finishes, &nexts] {
            CHECK(nexts == 2);
            finishes++;
          })));
      CHECK(nexts == 0);
      CHECK(finishes == 0);

      sub.Request(ElementCount(1));
      CHECK(nexts == 2);
      CHECK(finishes == 1);
    }
  }

  SECTION("cancel") {
    auto stream = InfiniteRange(0);

    bool next_called = false;
    AnySubscription sub = AnySubscription(stream.Subscribe(MakeSubscriber(
        [&next_called, &sub](int val) {
          CHECK(!next_called);
          next_called = true;
          sub.Cancel();
        },
        [](std::exception_ptr &&error) { CHECK(!"should not happen"); },
        [] { CHECK(!"should not happen"); })));
    sub.Request(ElementCount(0));
    CHECK(!next_called);
    sub.Request(ElementCount(1000));
    CHECK(next_called);
    sub.Request(ElementCount(1));
  }
}

}  // namespace shk
