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

#include <rs/publisher.h>

namespace shk {

TEST_CASE("Publisher") {
  static_assert(!IsPublisher<int>, "int is not a Publisher");

  SECTION("AnyPublisher") {
    SECTION("type traits") {
      auto pub = AnyPublisher<>(MakePublisher([](auto &&) {
        return MakeSubscription();
      }));

      static_assert(
          IsPublisher<decltype(pub)>,
          "MakePublisher should return Publisher");
    }

    SECTION("construct with lvalue reference") {
      auto inner_publisher = MakePublisher([](auto &&) {
        return MakeSubscription();
      });
      auto pub = AnyPublisher<>(inner_publisher);
    }

    SECTION("copyable") {
      auto pub = AnyPublisher<>(MakePublisher([](auto &&) {
        return MakeSubscription();
      }));
      AnyPublisher<> pub_copy(pub);
    }

    SECTION("assignable") {
      int original_pub_invoked = 0;
      auto pub = AnyPublisher<>(MakePublisher([&original_pub_invoked](auto &&) {
        original_pub_invoked++;
        return MakeSubscription();
      }));
      auto other_pub = AnyPublisher<>(MakePublisher([](auto &&) {
        CHECK(!"should not be invoked");
        return MakeSubscription();
      }));
      CHECK(&(other_pub = pub) == &other_pub);

      other_pub.Subscribe(MakeSubscriber());
      CHECK(original_pub_invoked == 1);

      pub.Subscribe(MakeSubscriber());
      CHECK(original_pub_invoked == 2);
    }

    SECTION("Subscribe") {
      int cancelled = 0;
      auto callback_pub = MakePublisher([&cancelled](auto &&subscriber) {
        subscriber.OnComplete();
        return MakeSubscription(
            [](ElementCount) {},
            [&cancelled] { cancelled++; });
      });
      const auto pub = AnyPublisher<int, std::string>(std::move(callback_pub));

      int completed = 0;
      auto sub = pub.Subscribe(MakeSubscriber(
          [](auto &&) {},
          [](std::exception_ptr &&) {},
          [&completed] { completed++; }));

      CHECK(completed == 1);
      CHECK(cancelled == 0);
      sub.Cancel();
      CHECK(cancelled == 1);
    }
  }

  SECTION("Callback MakePublisher") {
    int cancelled = 0;
    auto pub = MakePublisher([&cancelled](auto &&subscriber) mutable {
      subscriber.OnComplete();
      return MakeSubscription(
          [](ElementCount) {},
          [&cancelled] { cancelled++; });
    });

    SECTION("type traits") {
      static_assert(
          IsPublisher<decltype(pub)>,
          "MakePublisher should return Publisher");
    }

    SECTION("Subscribe") {
      int completed = 0;
      auto sub = pub.Subscribe(MakeSubscriber(
          [](auto &&) {},
          [](std::exception_ptr &&) {},
          [&completed] { completed++; }));

      CHECK(completed == 1);
      CHECK(cancelled == 0);
      sub.Cancel();
      CHECK(cancelled == 1);
    }

    SECTION("const Subscribe") {
      int cancelled = 0;
      const auto const_pub = MakePublisher([&cancelled](auto &&subscriber) {
        subscriber.OnComplete();
        return MakeSubscription(
            [](ElementCount) {},
            [&cancelled] { cancelled++; });
      });

      int completed = 0;
      auto sub = const_pub.Subscribe(MakeSubscriber(
          [](auto &&) {},
          [](std::exception_ptr &&) {},
          [&completed] { completed++; }));

      CHECK(completed == 1);
      CHECK(cancelled == 0);
      sub.Cancel();
      CHECK(cancelled == 1);
    }
  }
}

}  // namespace shk
