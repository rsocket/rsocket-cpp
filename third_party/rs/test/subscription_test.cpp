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

#include <rs/subscription.h>

namespace shk {
namespace {

class DummySubscription : public Subscription {
 public:
  DummySubscription() : last_called_(nullptr) {}

  explicit DummySubscription(DummySubscription **last_called)
      : last_called_(last_called) {}

  void Request(ElementCount count) {
    if (last_called_) {
      *last_called_ = this;
    }
  }

  void Cancel() {
    if (last_called_) {
      *last_called_ = this;
    }
  }

 private:
  DummySubscription **last_called_;
};

}  // anonymous namespace

TEST_CASE("Subscription") {
  SECTION("AnySubscription") {
    SECTION("type traits") {
      static_assert(
          IsSubscription<AnySubscription>,
          "AnySubscription must be a Subscription");
    }

    SECTION("default constructed") {
      AnySubscription sub;
      sub.Request(ElementCount(0));
      sub.Cancel();
    }

    SECTION("move") {
      auto sub = AnySubscription(MakeSubscription(
          [](ElementCount) {}, [] {}));
      auto moved_sub = std::move(sub);
    }

    SECTION("create from lvalue") {
      DummySubscription *last_called;
      auto dummy = DummySubscription(&last_called);

      auto sub = AnySubscription(dummy);

      sub.Request(ElementCount(0));
      CHECK(last_called != &dummy);  // dummy should be copied not held by ref
      sub.Cancel();
      CHECK(last_called != &dummy);  // dummy should be copied not held by ref
    }

    SECTION("Request") {
      ElementCount requested;
      {
        auto sub = AnySubscription(MakeSubscription(
            [&requested](ElementCount count) { requested += count; },
            [] { CHECK(!"Cancel should not be invoked"); }));
        CHECK(requested == 0);
        sub.Request(ElementCount(13));
        CHECK(requested == 13);
      }
      CHECK(requested == 13);
    }

    SECTION("Cancel") {
      bool cancelled = false;
      auto sub = AnySubscription(MakeSubscription(
          [](ElementCount) { CHECK(!"Request should not be invoked"); },
          [&cancelled] { CHECK(!cancelled); cancelled = true; }));
      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }
  }

  SECTION("VirtualSubscription") {
    SECTION("type traits") {
      static_assert(
          IsSubscription<VirtualSubscription<DummySubscription>>,
          "VirtualSubscription must be a Subscription");
    }

    SECTION("default constructed") {
      VirtualSubscription<DummySubscription> sub;
      sub.Request(ElementCount(0));
      sub.Cancel();
    }

    SECTION("move") {
      auto sub = MakeVirtualSubscription(MakeSubscription(
          [](ElementCount) {}, [] {}));
      auto moved_sub = std::move(sub);
    }

    SECTION("create from lvalue") {
      DummySubscription *last_called;
      auto dummy = DummySubscription(&last_called);

      auto sub = MakeVirtualSubscription(dummy);

      sub.Request(ElementCount(0));
      CHECK(last_called != &dummy);  // dummy should be copied not held by ref
      sub.Cancel();
      CHECK(last_called != &dummy);  // dummy should be copied not held by ref
    }

    SECTION("Request") {
      ElementCount requested;
      {
        auto sub = MakeVirtualSubscription(MakeSubscription(
            [&requested](ElementCount count) { requested += count; },
            [] { CHECK(!"Cancel should not be invoked"); }));
        CHECK(requested == 0);
        sub.Request(ElementCount(13));
        CHECK(requested == 13);
      }
      CHECK(requested == 13);
    }

    SECTION("Cancel") {
      bool cancelled = false;
      auto sub = MakeVirtualSubscription(MakeSubscription(
          [](ElementCount) { CHECK(!"Request should not be invoked"); },
          [&cancelled] { CHECK(!cancelled); cancelled = true; }));
      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }

    SECTION("MakeVirtualSubscriptionPtr") {
      SECTION("Request") {
        ElementCount requested;
        {
          std::unique_ptr<PureVirtualSubscription> sub =
              MakeVirtualSubscriptionPtr(MakeSubscription(
                  [&requested](ElementCount count) { requested += count; },
                  [] { CHECK(!"Cancel should not be invoked"); }));
          CHECK(requested == 0);
          sub->Request(ElementCount(13));
          CHECK(requested == 13);
        }
        CHECK(requested == 13);
      }

      SECTION("Cancel") {
        bool cancelled = false;
        std::unique_ptr<PureVirtualSubscription> sub =
            MakeVirtualSubscriptionPtr(MakeSubscription(
                [](ElementCount) { CHECK(!"Request should not be invoked"); },
                [&cancelled] { CHECK(!cancelled); cancelled = true; }));
        CHECK(!cancelled);
        sub->Cancel();
        CHECK(cancelled);
      }
    }
  }

  SECTION("Dummy MakeSubscription") {
    auto dummy = MakeSubscription();

    SECTION("Move") {
      auto moved_dummy = std::move(dummy);
    }

    SECTION("Request") {
      dummy.Request(ElementCount(10));
    }

    SECTION("Cancel") {
      dummy.Cancel();
    }
  }

  SECTION("Callback MakeSubscription") {
    SECTION("Move") {
      auto sub = MakeSubscription(
          [](ElementCount) {}, [] {});
      auto moved_sub = std::move(sub);
    }

    SECTION("default constructor") {
      auto sub = MakeSubscription(
          [a = std::make_unique<int>(0)](ElementCount) {}, [] {});
      decltype(sub) default_constructed_sub;
      default_constructed_sub.Request(ElementCount(13));
      default_constructed_sub.Cancel();
    }

    SECTION("Request") {
      ElementCount requested;
      {
        auto sub = MakeSubscription(
            [&requested](ElementCount count) { requested += count; },
            [] { CHECK(!"Cancel should not be invoked"); });
        CHECK(requested == 0);
        sub.Request(ElementCount(13));
        CHECK(requested == 13);
      }
      CHECK(requested == 13);
    }

    SECTION("Cancel") {
      bool cancelled = false;
      auto sub = MakeSubscription(
          [](ElementCount) { CHECK(!"Request should not be invoked"); },
          [&cancelled] { CHECK(!cancelled); cancelled = true; });
      CHECK(!cancelled);
      sub.Cancel();
      CHECK(cancelled);
    }
  }
}

}  // namespace shk
